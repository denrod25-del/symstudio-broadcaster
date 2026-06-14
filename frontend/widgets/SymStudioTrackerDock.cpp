#include "SymStudioTrackerDock.hpp"
#include "OBSApp.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QPointer>
#include <QPixmap>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

#include <util/config-file.h>

static QString uptimeStr(const QString &startedIso)
{
	QDateTime st = QDateTime::fromString(startedIso, Qt::ISODate);
	if (!st.isValid())
		return QString();
	qint64 secs = st.secsTo(QDateTime::currentDateTimeUtc());
	if (secs < 0)
		secs = 0;
	return QStringLiteral("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60);
}

SymStudioTrackerDock::SymStudioTrackerDock(QWidget *parent) : QWidget(parent)
{
	nam = new QNetworkAccessManager(this);
	pollTimer = new QTimer(this);
	pollTimer->setInterval(45000);
	connect(pollTimer, &QTimer::timeout, this, &SymStudioTrackerDock::onPoll);

	QVBoxLayout *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	QHBoxLayout *top = new QHBoxLayout();
	watchEdit = new QLineEdit(this);
	watchEdit->setPlaceholderText(QStringLiteral("channels: comma or space separated"));
	saveBtn = new QPushButton(QStringLiteral("Save"), this);
	refreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
	top->addWidget(watchEdit, 1);
	top->addWidget(saveBtn);
	top->addWidget(refreshBtn);
	root->addLayout(top);

	headerLabel = new QLabel(this);
	headerLabel->setStyleSheet(QStringLiteral("color:#00E5FF;font-weight:bold;"));
	root->addWidget(headerLabel);

	statusLabel = new QLabel(this);
	statusLabel->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	root->addWidget(statusLabel);

	board = new QWidget(this);
	boardLayout = new QVBoxLayout(board);
	boardLayout->setContentsMargins(0, 0, 0, 0);
	boardLayout->setSpacing(6);
	boardLayout->addStretch(1);
	scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setWidget(board);
	root->addWidget(scroll, 1);

	setLayout(root);

	connect(saveBtn, &QPushButton::clicked, this, &SymStudioTrackerDock::onSave);
	connect(refreshBtn, &QPushButton::clicked, this, &SymStudioTrackerDock::onPoll);

	loadConfig();
	if (!channels.isEmpty())
		onPoll();
	pollTimer->start();
}

void SymStudioTrackerDock::setStatus(const QString &text)
{
	statusLabel->setText(text);
}

void SymStudioTrackerDock::loadConfig()
{
	const char *c = config_get_string(App()->GetUserConfig(), "SymStudioTracker", "Channels");
	if (c && *c) {
		watchEdit->setText(QString::fromUtf8(c));
		channels = QString::fromUtf8(c).split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
	}
}

void SymStudioTrackerDock::onSave()
{
	const QString raw = watchEdit->text().trimmed();
	channels = raw.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
	config_set_string(App()->GetUserConfig(), "SymStudioTracker", "Channels", raw.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	onPoll();
}

void SymStudioTrackerDock::onPoll()
{
	const char *tokC = config_get_string(App()->GetUserConfig(), "SymStudioTwitch", "AccessToken");
	const char *idC = config_get_string(App()->GetUserConfig(), "SymStudioTwitch", "ClientID");
	const QString token = tokC ? QString::fromUtf8(tokC) : QString();
	const QString clientId = idC ? QString::fromUtf8(idC) : QString();
	if (token.isEmpty() || clientId.isEmpty()) {
		setStatus(QStringLiteral("Log in via the Stream Info dock first."));
		return;
	}
	if (channels.isEmpty()) {
		setStatus(QStringLiteral("Add channels above, then Save."));
		return;
	}

	QUrl url(QStringLiteral("https://api.twitch.tv/helix/streams"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("first"), QStringLiteral("100"));
	for (const QString &ch : channels)
		q.addQueryItem(QStringLiteral("user_login"), ch.toLower());
	url.setQuery(q);

	QNetworkRequest req(url);
	req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
	req.setRawHeader("Client-Id", clientId.toUtf8());
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (code == 401) {
			setStatus(QStringLiteral("Session expired — re-login in the Stream Info dock."));
			return;
		}
		const QJsonArray data = QJsonDocument::fromJson(reply->readAll())
						.object().value(QStringLiteral("data")).toArray();
		currentStreams.clear();
		for (const QJsonValue &v : data) {
			const QJsonObject o = v.toObject();
			Stream s;
			s.name = o.value(QStringLiteral("user_name")).toString();
			s.viewers = o.value(QStringLiteral("viewer_count")).toInt();
			s.game = o.value(QStringLiteral("game_name")).toString();
			s.title = o.value(QStringLiteral("title")).toString();
			s.started = o.value(QStringLiteral("started_at")).toString();
			s.thumbUrl = o.value(QStringLiteral("thumbnail_url")).toString();
			currentStreams.insert(o.value(QStringLiteral("user_login")).toString().toLower(), s);
		}
		setStatus(QStringLiteral("Updated %1").arg(QTime::currentTime().toString("HH:mm:ss")));
		rebuildBoard();
	});
}

void SymStudioTrackerDock::rebuildBoard()
{
	while (boardLayout->count() > 1) {
		QLayoutItem *it = boardLayout->takeAt(0);
		if (it->widget())
			it->widget()->deleteLater();
		delete it;
	}

	QStringList live, offline;
	for (const QString &ch : channels) {
		const QString key = ch.toLower();
		if (currentStreams.contains(key))
			live.append(key);
		else
			offline.append(key);
	}
	std::sort(live.begin(), live.end(), [this](const QString &a, const QString &b) {
		return currentStreams[a].viewers > currentStreams[b].viewers;
	});

	int liveCount = 0;
	long long total = 0;
	auto addTile = [this](const QString &key, bool isLive) {
		const Stream *s = isLive ? &currentStreams[key] : nullptr;
		QFrame *f = new QFrame(board);
		f->setStyleSheet(QStringLiteral("QFrame{background:#10141E;border:1px solid #1A2230;border-radius:6px;}"));
		QHBoxLayout *h = new QHBoxLayout(f);
		h->setContentsMargins(8, 6, 8, 6);

		QLabel *thumb = new QLabel(f);
		thumb->setFixedSize(120, 68);
		thumb->setStyleSheet(QStringLiteral("background:#06080E;border-radius:4px;"));
		h->addWidget(thumb);

		QVBoxLayout *info = new QVBoxLayout();
		const QString headHtml = isLive
			? QStringLiteral("<b style='color:#39FF6A'>● LIVE</b>  %1").arg(s->name.toHtmlEscaped())
			: QStringLiteral("<span style='color:#6E7A8C'>● offline</span>  %1").arg(key.toHtmlEscaped());
		QLabel *l1 = new QLabel(headHtml, f);
		l1->setTextFormat(Qt::RichText);
		info->addWidget(l1);
		if (isLive) {
			const int prev = lastViewers.value(key, -1);
			const int delta = (prev >= 0) ? s->viewers - prev : 0;
			QString d;
			if (delta > 0)
				d = QStringLiteral(" <span style='color:#39FF6A'>▲%1</span>").arg(delta);
			else if (delta < 0)
				d = QStringLiteral(" <span style='color:#FF5046'>▼%1</span>").arg(-delta);
			QLabel *l2 = new QLabel(QStringLiteral("<b style='color:#00E5FF'>%1</b> viewers%2 · %3")
							.arg(QString::number(s->viewers), d, s->game.toHtmlEscaped()), f);
			l2->setTextFormat(Qt::RichText);
			info->addWidget(l2);
			QLabel *l3 = new QLabel(QStringLiteral("%1 · up %2")
							.arg(s->title.left(60).toHtmlEscaped(), uptimeStr(s->started)), f);
			l3->setStyleSheet(QStringLiteral("color:#9AA3B2;font-size:11px;"));
			l3->setWordWrap(true);
			info->addWidget(l3);
			lastViewers[key] = s->viewers;
			fetchThumb(thumb, s->thumbUrl);
		}
		info->addStretch(1);
		h->addLayout(info, 1);
		boardLayout->insertWidget(boardLayout->count() - 1, f);
	};

	for (const QString &key : live) {
		liveCount++;
		total += currentStreams[key].viewers;
		addTile(key, true);
	}
	for (const QString &key : offline)
		addTile(key, false);

	headerLabel->setText(QStringLiteral("Watchlist — %1 live · %2 viewers").arg(liveCount).arg(total));
}

void SymStudioTrackerDock::fetchThumb(QLabel *label, const QString &urlTemplate)
{
	if (urlTemplate.isEmpty())
		return;
	QString url = urlTemplate;
	url.replace(QStringLiteral("{width}"), QStringLiteral("240"));
	url.replace(QStringLiteral("{height}"), QStringLiteral("135"));
	QPointer<QLabel> guard(label);
	QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(url)));
	connect(reply, &QNetworkReply::finished, this, [reply, guard]() {
		reply->deleteLater();
		if (!guard)
			return;
		QPixmap pm;
		if (pm.loadFromData(reply->readAll()) && !pm.isNull())
			guard->setPixmap(pm.scaled(guard->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	});
}
