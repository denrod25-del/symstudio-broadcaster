#pragma once

#include <QWidget>
#include <QByteArray>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QTextEdit;
class QTcpSocket;
class QTimer;

// SymStudio Twitch alerts dock — anonymous IRC USERNOTICE/bits -> feed + on-canvas text.
class SymStudioAlertsDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioAlertsDock(QWidget *parent = nullptr);

private slots:
	void onConnectClicked();
	void onSocketConnected();
	void onSocketReadyRead();
	void onSocketDisconnected();
	void onTestClicked();
	void clearCanvasText();

private:
	void setConnectedState(bool on);
	void loadConfig();
	void saveStr(const char *key, const QString &val);
	void handleLine(const QString &line);
	void addAlert(const QString &text);
	void updateCanvas(const QString &text);

	QLineEdit *channelEdit = nullptr;
	QPushButton *connectBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QCheckBox *canvasCheck = nullptr;
	QPushButton *testBtn = nullptr;
	QTextEdit *feed = nullptr;

	QTcpSocket *socket = nullptr;
	QTimer *clearTimer = nullptr;
	QByteArray rxBuffer;
	QString channel;
	bool connected = false;
};
