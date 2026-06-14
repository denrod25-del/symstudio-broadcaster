#pragma once

#include <QWidget>
#include <QByteArray>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QTextEdit;
class QTcpSocket;

// SymStudio Twitch chat dock — anonymous read-only IRC feed.
class SymStudioChatDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioChatDock(QWidget *parent = nullptr);
	~SymStudioChatDock() override;

private slots:
	void onConnectClicked();
	void onSocketConnected();
	void onSocketReadyRead();
	void onSocketDisconnected();

private:
	void setConnectedState(bool on);
	void appendSystem(const QString &text);
	void appendMessage(const QString &name, const QString &color, const QString &msg);
	void handleLine(const QString &line);

	QLineEdit *channelEdit = nullptr;
	QPushButton *connectBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QTextEdit *chatView = nullptr;
	QTcpSocket *socket = nullptr;
	QByteArray rxBuffer;
	QString channel;
	bool connected = false;
};
