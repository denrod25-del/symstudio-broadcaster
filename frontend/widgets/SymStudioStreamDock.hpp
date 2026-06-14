#pragma once

#include <QWidget>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QNetworkAccessManager;
class QTimer;

// SymStudio Twitch stream-info dock — device-code OAuth + Helix title/category updater.
class SymStudioStreamDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioStreamDock(QWidget *parent = nullptr);

private slots:
	void onSaveClientId();
	void onLoginClicked();
	void onLogoutClicked();
	void onPollTick();
	void onSearchCategory();
	void onUpdateClicked();

private:
	void loadConfig();
	void saveStr(const char *key, const QString &val);
	void updateUiState();
	void setStatus(const QString &text);

	void startDeviceFlow();
	void fetchUser();
	void loadChannelInfo();
	bool tryRefreshThen();

	QString clientId, accessToken, refreshToken, broadcasterId, login, deviceCode;
	QString selectedGameId;

	QLineEdit *clientIdEdit = nullptr;
	QPushButton *saveIdBtn = nullptr;
	QPushButton *loginBtn = nullptr;
	QPushButton *logoutBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QLineEdit *titleEdit = nullptr;
	QLineEdit *categoryEdit = nullptr;
	QPushButton *searchBtn = nullptr;
	QComboBox *categoryResults = nullptr;
	QPushButton *updateBtn = nullptr;

	QNetworkAccessManager *nam = nullptr;
	QTimer *pollTimer = nullptr;
	int pollExpiry = 0;
};
