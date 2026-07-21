#include "SymStudioEventSub.hpp"

#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QtEndian>

SymStudioEventSub::SymStudioEventSub(QObject *parent) : QObject(parent)
{
	nam = new QNetworkAccessManager(this);
	watchdog = new QTimer(this);
	watchdog->setSingleShot(true);
	connect(watchdog, &QTimer::timeout, this, &SymStudioEventSub::onWatchdog);
}

SymStudioEventSub::~SymStudioEventSub()
{
	stop();
}

void SymStudioEventSub::start(const QString &b, const QString &t, const QString &c)
{
	++connectGen; // invalidate any pending reconnect from a prior session
	broadcasterId = b;
	token = t;
	clientId = c;
	if (broadcasterId.isEmpty() || token.isEmpty() || clientId.isEmpty())
		return;
	reconnectDelayMs = 1000;
	connectSocket(QStringLiteral("eventsub.wss.twitch.tv"), QStringLiteral("/ws"));
}

void SymStudioEventSub::stop()
{
	++connectGen; // any queued reconnect lambda sees a stale gen and bails
	live = false;
	if (watchdog)
		watchdog->stop();
	if (sock) {
		sock->disconnect(this);
		sock->abort();
		sock->deleteLater();
		sock = nullptr;
	}
	rx.clear();
	msgBuf.clear();
	handshakeDone = false;
	sessionId.clear();
	// Clear credentials so nothing can silently reconnect after an explicit stop.
	broadcasterId.clear();
	token.clear();
	clientId.clear();
}

void SymStudioEventSub::connectSocket(const QString &host, const QString &path)
{
	if (sock) {
		sock->disconnect(this);
		sock->abort();
		sock->deleteLater();
	}
	rx.clear();
	msgBuf.clear();
	handshakeDone = false;
	wsHost = host;
	wsPath = path;
	sock = new QSslSocket(this);
	connect(sock, &QSslSocket::encrypted, this, &SymStudioEventSub::onEncrypted);
	connect(sock, &QSslSocket::readyRead, this, &SymStudioEventSub::onReadyRead);
	connect(sock, &QSslSocket::disconnected, this, &SymStudioEventSub::onSocketError);
	connect(sock, &QAbstractSocket::errorOccurred, this, &SymStudioEventSub::onSocketError);
	emit status(QStringLiteral("EventSub: connecting…"));
	sock->connectToHostEncrypted(wsHost, 443);
}

void SymStudioEventSub::sendHandshake()
{
	QByteArray key = QByteArray::number((qint64)QDateTime::currentMSecsSinceEpoch())
				 .append(QByteArray::number((qint64)(quintptr)this))
				 .toBase64()
				 .left(24);
	QByteArray req = "GET " + wsPath.toUtf8() + " HTTP/1.1\r\n";
	req += "Host: " + wsHost.toUtf8() + "\r\n";
	req += "Upgrade: websocket\r\n";
	req += "Connection: Upgrade\r\n";
	req += "Sec-WebSocket-Key: " + key + "\r\n";
	req += "Sec-WebSocket-Version: 13\r\n\r\n";
	sock->write(req);
}

void SymStudioEventSub::onEncrypted()
{
	sendHandshake();
}

void SymStudioEventSub::onReadyRead()
{
	rx += sock->readAll();

	if (!handshakeDone) {
		int end = rx.indexOf("\r\n\r\n");
		if (end < 0)
			return; // wait for full headers
		QByteArray head = rx.left(end);
		rx.remove(0, end + 4);
		if (!head.startsWith("HTTP/1.1 101")) {
			emit status(QStringLiteral("EventSub: handshake failed"));
			scheduleReconnect();
			return;
		}
		handshakeDone = true;
	}

	while (true) {
		if (rx.size() < 2)
			return;
		const quint8 b0 = (quint8)rx[0];
		const quint8 b1 = (quint8)rx[1];
		const bool fin = b0 & 0x80;
		const quint8 opcode = b0 & 0x0F;
		const bool masked = b1 & 0x80; // server->client should be 0
		quint64 len = b1 & 0x7F;
		int pos = 2;
		if (len == 126) {
			if (rx.size() < pos + 2)
				return;
			len = qFromBigEndian<quint16>((const uchar *)rx.constData() + pos);
			pos += 2;
		} else if (len == 127) {
			if (rx.size() < pos + 8)
				return;
			len = qFromBigEndian<quint64>((const uchar *)rx.constData() + pos);
			pos += 8;
		}
		// Sanity ceiling: EventSub frames are tiny. A frame claiming a huge length is
		// a protocol error (or a truncated/corrupt stream) — never wait to buffer it.
		if (len > (16u * 1024u * 1024u)) {
			emit status(QStringLiteral("EventSub: oversized frame — reconnecting"));
			scheduleReconnect();
			return;
		}
		const int maskLen = masked ? 4 : 0;
		if ((quint64)rx.size() < (quint64)pos + maskLen + len)
			return; // wait for the full payload
		QByteArray payload = rx.mid(pos + maskLen, (int)len);
		if (masked) {
			const char *mk = rx.constData() + pos;
			for (int i = 0; i < payload.size(); i++)
				payload[i] = payload[i] ^ mk[i % 4];
		}
		rx.remove(0, pos + maskLen + (int)len);

		if (opcode == 0x0 || opcode == 0x1) { // continuation / text
			if (opcode == 0x1) {
				msgBuf.clear();
				msgOpcode = 0x1;
			}
			msgBuf += payload;
			if (fin && msgOpcode == 0x1) {
				handleMessage(msgBuf);
				msgBuf.clear();
			}
		} else if (opcode == 0x9) { // ping -> pong
			sendControl(0xA, payload);
		} else if (opcode == 0x8) { // close
			emit status(QStringLiteral("EventSub: closed by server"));
			scheduleReconnect();
			return;
		}
		// 0xA (pong) ignored
	}
}

void SymStudioEventSub::sendControl(quint8 opcode, const QByteArray &payload)
{
	if (!sock || sock->state() != QAbstractSocket::ConnectedState)
		return;
	QByteArray f;
	f.append((char)(0x80 | opcode));
	const int n = payload.size();
	const quint8 mask = 0x80;
	if (n < 126) {
		f.append((char)(mask | n));
	} else if (n < 65536) {
		f.append((char)(mask | 126));
		f.append((char)((n >> 8) & 0xFF));
		f.append((char)(n & 0xFF));
	} else {
		f.append((char)(mask | 127));
		for (int i = 7; i >= 0; i--)
			f.append((char)((n >> (i * 8)) & 0xFF));
	}
	char mk[4];
	const quint64 r = (quint64)QDateTime::currentMSecsSinceEpoch() ^ (quint64)(quintptr)this;
	for (int i = 0; i < 4; i++)
		mk[i] = (char)((r >> (i * 8)) & 0xFF);
	f.append(mk, 4);
	QByteArray p = payload;
	for (int i = 0; i < p.size(); i++)
		p[i] = p[i] ^ mk[i % 4];
	f.append(p);
	sock->write(f);
}

void SymStudioEventSub::onSocketError()
{
	if (live || handshakeDone || (sock && sock->state() != QAbstractSocket::ConnectedState))
		scheduleReconnect();
}

void SymStudioEventSub::scheduleReconnect()
{
	live = false;
	handshakeDone = false;
	if (watchdog)
		watchdog->stop(); // don't let a stale keepalive watchdog schedule a second cycle
	if (sock) {
		sock->disconnect(this);
		sock->abort();
		sock->deleteLater();
		sock = nullptr;
	}
	emit status(QStringLiteral("EventSub: reconnecting…"));
	const int delay = reconnectDelayMs;
	reconnectDelayMs = qMin(reconnectDelayMs * 2, 30000);
	const quint64 gen = connectGen;
	QTimer::singleShot(delay, this, [this, gen]() {
		if (gen == connectGen && !token.isEmpty())
			connectSocket(QStringLiteral("eventsub.wss.twitch.tv"), QStringLiteral("/ws"));
	});
}

void SymStudioEventSub::onWatchdog()
{
	emit status(QStringLiteral("EventSub: silent — reconnecting"));
	scheduleReconnect();
}

void SymStudioEventSub::handleMessage(const QByteArray &json)
{
	// Any message resets the keepalive watchdog (keepalive + slack).
	watchdog->start((keepaliveSecs + 10) * 1000);

	const QJsonObject root = QJsonDocument::fromJson(json).object();
	const QJsonObject meta = root.value(QStringLiteral("metadata")).toObject();
	const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
	const QString type = meta.value(QStringLiteral("message_type")).toString();

	if (type == QStringLiteral("session_welcome")) {
		const QJsonObject s = payload.value(QStringLiteral("session")).toObject();
		sessionId = s.value(QStringLiteral("id")).toString();
		keepaliveSecs = s.value(QStringLiteral("keepalive_timeout_seconds")).toInt(30);
		watchdog->start((keepaliveSecs + 10) * 1000);
		reconnectDelayMs = 1000;
		createSubscriptions();
	} else if (type == QStringLiteral("session_reconnect")) {
		const QJsonObject s = payload.value(QStringLiteral("session")).toObject();
		const QUrl u(s.value(QStringLiteral("reconnect_url")).toString());
		if (u.isValid()) {
			emit status(QStringLiteral("EventSub: reconnecting (session)…"));
			connectSocket(u.host(), u.path() + (u.hasQuery() ? "?" + u.query() : QString()));
		}
	} else if (type == QStringLiteral("notification")) {
		const QString sub = meta.value(QStringLiteral("subscription_type")).toString();
		const QJsonObject e = payload.value(QStringLiteral("event")).toObject();
		auto name = [&](const char *k) { return e.value(QString::fromUtf8(k)).toString(); };
		QString text, kind;
		if (sub == QStringLiteral("channel.follow")) {
			text = QStringLiteral("%1 followed!").arg(name("user_name"));
			kind = QStringLiteral("follow");
		} else if (sub == QStringLiteral("channel.subscribe")) {
			const QString tier = name("tier");
			text = QStringLiteral("%1 subscribed (Tier %2)!").arg(name("user_name"), tier.left(1));
			kind = QStringLiteral("sub");
		} else if (sub == QStringLiteral("channel.subscription.message")) {
			const int months = e.value(QStringLiteral("cumulative_months")).toInt();
			text = QStringLiteral("%1 resubscribed (%2 months)!").arg(name("user_name")).arg(months);
			kind = QStringLiteral("resub");
		} else if (sub == QStringLiteral("channel.subscription.gift")) {
			const int total = e.value(QStringLiteral("total")).toInt();
			const QString who = e.value(QStringLiteral("is_anonymous")).toBool()
						    ? QStringLiteral("Someone")
						    : name("user_name");
			text = QStringLiteral("%1 gifted %2 subs!").arg(who).arg(total);
			kind = QStringLiteral("subgift");
		} else if (sub == QStringLiteral("channel.cheer")) {
			const int bits = e.value(QStringLiteral("bits")).toInt();
			const QString who = e.value(QStringLiteral("is_anonymous")).toBool()
						    ? QStringLiteral("Someone")
						    : name("user_name");
			text = QStringLiteral("%1 cheered %2 bits!").arg(who).arg(bits);
			kind = QStringLiteral("cheer");
		} else if (sub == QStringLiteral("channel.raid")) {
			const int v = e.value(QStringLiteral("viewers")).toInt();
			text = QStringLiteral("%1 raided with %2 viewers!")
				       .arg(name("from_broadcaster_user_name")).arg(v);
			kind = QStringLiteral("raid");
		}
		if (!text.isEmpty())
			emit alert(text, kind);
	}
}

void SymStudioEventSub::subscribe(const QString &type, const QString &version, const QByteArray &conditionJson)
{
	subTotal++;
	QByteArray body = "{\"type\":\"" + type.toUtf8() + "\",\"version\":\"" + version.toUtf8() +
			  "\",\"condition\":" + conditionJson +
			  ",\"transport\":{\"method\":\"websocket\",\"session_id\":\"" + sessionId.toUtf8() +
			  "\"}}";
	QNetworkRequest req(QUrl(QStringLiteral("https://api.twitch.tv/helix/eventsub/subscriptions")));
	req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
	req.setRawHeader("Client-Id", clientId.toUtf8());
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	QNetworkReply *reply = nam->post(req, body);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		// 409 = subscription already exists (happens on session_reconnect, where the
		// migrated session keeps its subs). Treat it as success so status stays "live".
		if (code == 202 || code == 200 || code == 409) {
			subOk++;
		} else if (code == 401) {
			emit status(QStringLiteral("EventSub: session expired — re-login in Stream Info"));
		}
		live = subOk > 0;
		emit status(QStringLiteral("EventSub: live (%1/%2 subscribed)").arg(subOk).arg(subTotal));
	});
}

void SymStudioEventSub::createSubscriptions()
{
	subOk = 0;
	subTotal = 0;
	const QByteArray b = broadcasterId.toUtf8();
	const QByteArray bcOnly = "{\"broadcaster_user_id\":\"" + b + "\"}";
	subscribe(QStringLiteral("channel.follow"), QStringLiteral("2"),
		  "{\"broadcaster_user_id\":\"" + b + "\",\"moderator_user_id\":\"" + b + "\"}");
	subscribe(QStringLiteral("channel.subscribe"), QStringLiteral("1"), bcOnly);
	subscribe(QStringLiteral("channel.subscription.message"), QStringLiteral("1"), bcOnly);
	subscribe(QStringLiteral("channel.subscription.gift"), QStringLiteral("1"), bcOnly);
	subscribe(QStringLiteral("channel.cheer"), QStringLiteral("1"), bcOnly);
	subscribe(QStringLiteral("channel.raid"), QStringLiteral("1"),
		  "{\"to_broadcaster_user_id\":\"" + b + "\"}");
}
