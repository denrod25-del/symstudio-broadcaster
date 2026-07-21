#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class QSslSocket;
class QNetworkAccessManager;
class QTimer;

// Real-time Twitch alerts via a hand-rolled EventSub WebSocket (over QSslSocket).
// Receive-only text frames; emits alert(text,type) for the Alerts dock.
class SymStudioEventSub : public QObject {
	Q_OBJECT

public:
	explicit SymStudioEventSub(QObject *parent = nullptr);
	~SymStudioEventSub() override;

	// Begin/refresh the connection for a logged-in broadcaster.
	void start(const QString &broadcasterId, const QString &token, const QString &clientId);
	void stop();
	bool isLive() const { return live; }

signals:
	void alert(const QString &text, const QString &type);
	void status(const QString &text);

private slots:
	void onEncrypted();
	void onReadyRead();
	void onSocketError();
	void onWatchdog();

private:
	void connectSocket(const QString &host, const QString &path);
	void sendHandshake();
	void sendControl(quint8 opcode, const QByteArray &payload); // masked
	void handleMessage(const QByteArray &json);
	void createSubscriptions();
	void subscribe(const QString &type, const QString &version, const QByteArray &conditionJson);
	void scheduleReconnect();

	QSslSocket *sock = nullptr;
	QNetworkAccessManager *nam = nullptr;
	QTimer *watchdog = nullptr;

	QString broadcasterId, token, clientId;
	QString wsHost = QStringLiteral("eventsub.wss.twitch.tv");
	QString wsPath = QStringLiteral("/ws");
	QString sessionId;
	QByteArray rx;     // raw socket bytes
	QByteArray msgBuf; // reassembled fragmented text payload
	quint8 msgOpcode = 0;
	bool handshakeDone = false;
	bool live = false;
	int keepaliveSecs = 30;
	int subOk = 0, subTotal = 0;
	int reconnectDelayMs = 1000;
	quint64 connectGen = 0; // bumped on start()/stop() to cancel stale pending reconnects
};
