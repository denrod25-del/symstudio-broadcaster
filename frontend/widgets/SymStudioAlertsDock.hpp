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
class SymStudioAlertServer;
class SymStudioEventSub;

// SymStudio Twitch alerts dock — anonymous IRC USERNOTICE/bits -> feed + on-canvas text.
class SymStudioAlertsDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioAlertsDock(QWidget *parent = nullptr);
	~SymStudioAlertsDock() override;

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
	void addAlert(const QString &text, const QString &type = QStringLiteral("alert"));
	void updateCanvas(const QString &text);
	void ensureOverlaySource();

	QLineEdit *channelEdit = nullptr;
	QPushButton *connectBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QCheckBox *canvasCheck = nullptr;
	QCheckBox *overlayCheck = nullptr;
	QPushButton *testBtn = nullptr;
	QTextEdit *feed = nullptr;
	QLabel *overlayInfo = nullptr;

	SymStudioAlertServer *alertServer = nullptr;
	SymStudioEventSub *eventSub = nullptr;
	bool eventSubActive = false;
	QTcpSocket *socket = nullptr;
	QTimer *clearTimer = nullptr;
	QByteArray rxBuffer;
	QString channel;
	bool connected = false;
};
