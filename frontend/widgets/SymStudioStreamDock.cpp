#include "SymStudioStreamDock.hpp"
#include "OBSApp.hpp"
#include "SymStudioTwitch.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>

#include <util/config-file.h>

SymStudioStreamDock::SymStudioStreamDock(QWidget *parent) : QWidget(parent)
{
	nam = new QNetworkAccessManager(this);
	pollTimer = new QTimer(this);
	connect(pollTimer, &QTimer::timeout, this, &SymStudioStreamDock::onPollTick);

	QVBoxLayout *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	QHBoxLayout *authRow = new QHBoxLayout();
	loginBtn = new QPushButton(QStringLiteral("Login to Twitch"), this);
	logoutBtn = new QPushButton(QStringLiteral("Log out"), this);
	authRow->addWidget(loginBtn);
	authRow->addWidget(logoutBtn);
	root->addLayout(authRow);

	statusLabel = new QLabel(this);
	statusLabel->setWordWrap(true);
	statusLabel->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	root->addWidget(statusLabel);

	titleEdit = new QLineEdit(this);
	titleEdit->setPlaceholderText(QStringLiteral("Stream title"));
	root->addWidget(titleEdit);

	QHBoxLayout *catRow = new QHBoxLayout();
	categoryEdit = new QLineEdit(this);
	categoryEdit->setPlaceholderText(QStringLiteral("Category / game"));
	searchBtn = new QPushButton(QStringLiteral("Search"), this);
	catRow->addWidget(categoryEdit, 1);
	catRow->addWidget(searchBtn);
	root->addLayout(catRow);

	categoryResults = new QComboBox(this);
	root->addWidget(categoryResults);

	updateBtn = new QPushButton(QStringLiteral("Update Stream Info"), this);
	root->addWidget(updateBtn);

	root->addStretch(1);
	setLayout(root);

	connect(loginBtn, &QPushButton::clicked, this, &SymStudioStreamDock::onLoginClicked);
	connect(logoutBtn, &QPushButton::clicked, this, &SymStudioStreamDock::onLogoutClicked);
	connect(searchBtn, &QPushButton::clicked, this, &SymStudioStreamDock::onSearchCategory);
	connect(updateBtn, &QPushButton::clicked, this, &SymStudioStreamDock::onUpdateClicked);

	loadConfig();
	updateUiState();
}

void SymStudioStreamDock::setStatus(const QString &text)
{
	statusLabel->setText(text);
}

void SymStudioStreamDock::saveStr(const char *key, const QString &val)
{
	config_set_string(App()->GetUserConfig(), "SymStudioTwitch", key, val.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

void SymStudioStreamDock::loadConfig()
{
	auto get = [](const char *k) -> QString {
		const char *v = config_get_string(App()->GetUserConfig(), "SymStudioTwitch", k);
		return v ? QString::fromUtf8(v) : QString();
	};
	accessToken = get("AccessToken");
	refreshToken = get("RefreshToken");
	broadcasterId = get("BroadcasterId");
	login = get("Login");

	// One-time migration: a token issued under a different (user-registered)
	// client ID won't work with the shared SymStudio app, so clear it and force
	// a clean re-login. New users have no stored ClientID and are unaffected.
	const QString storedId = get("ClientID");
	if (!storedId.isEmpty() && storedId != QStringLiteral(SYMSTUDIO_TWITCH_CLIENT_ID)) {
		accessToken.clear();
		refreshToken.clear();
		broadcasterId.clear();
		login.clear();
		saveStr("AccessToken", "");
		saveStr("RefreshToken", "");
		saveStr("BroadcasterId", "");
		saveStr("Login", "");
	}

	clientId = QStringLiteral(SYMSTUDIO_TWITCH_CLIENT_ID);
	saveStr("ClientID", clientId);
}

void SymStudioStreamDock::updateUiState()
{
	const bool loggedIn = !accessToken.isEmpty() && !broadcasterId.isEmpty();

	loginBtn->setVisible(!loggedIn);
	logoutBtn->setVisible(loggedIn);
	titleEdit->setEnabled(loggedIn);
	categoryEdit->setEnabled(loggedIn);
	searchBtn->setEnabled(loggedIn);
	categoryResults->setEnabled(loggedIn);
	updateBtn->setEnabled(loggedIn);

	if (loggedIn)
		setStatus(QStringLiteral("Logged in as %1").arg(login));
	else
		setStatus(QStringLiteral("Click Login to connect your Twitch account."));
}

void SymStudioStreamDock::onLogoutClicked()
{
	accessToken.clear();
	refreshToken.clear();
	broadcasterId.clear();
	login.clear();
	saveStr("AccessToken", "");
	saveStr("RefreshToken", "");
	saveStr("BroadcasterId", "");
	saveStr("Login", "");
	updateUiState();
}

void SymStudioStreamDock::onLoginClicked()
{
	if (clientId.isEmpty())
		return;
	startDeviceFlow();
}

void SymStudioStreamDock::startDeviceFlow()
{
	setStatus(QStringLiteral("Requesting device code…"));
	QNetworkRequest req(QUrl(QStringLiteral("https://id.twitch.tv/oauth2/device")));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("client_id"), clientId);
	q.addQueryItem(QStringLiteral("scopes"), QStringLiteral(SYMSTUDIO_TWITCH_SCOPES));
	QNetworkReply *reply = nam->post(req, q.toString(QUrl::FullyEncoded).toUtf8());
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
		deviceCode = o.value(QStringLiteral("device_code")).toString();
		const QString userCode = o.value(QStringLiteral("user_code")).toString();
		const QString verify = o.value(QStringLiteral("verification_uri")).toString();
		int interval = o.value(QStringLiteral("interval")).toInt(5);
		pollExpiry = o.value(QStringLiteral("expires_in")).toInt(1800);
		if (deviceCode.isEmpty() || userCode.isEmpty()) {
			setStatus(QStringLiteral("Login failed (check Client ID + Device Code Grant enabled)."));
			return;
		}
		const QString uri = verify.isEmpty() ? QStringLiteral("https://www.twitch.tv/activate") : verify;
		setStatus(QStringLiteral("Go to %1 and enter code: %2").arg(uri, userCode));
		QDesktopServices::openUrl(QUrl(uri));
		pollTimer->start(qMax(1, interval) * 1000);
	});
}

void SymStudioStreamDock::onPollTick()
{
	pollExpiry -= pollTimer->interval() / 1000;
	if (pollExpiry <= 0) {
		pollTimer->stop();
		setStatus(QStringLiteral("Code expired — click Login to try again."));
		return;
	}
	QNetworkRequest req(QUrl(QStringLiteral("https://id.twitch.tv/oauth2/token")));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("client_id"), clientId);
	q.addQueryItem(QStringLiteral("scopes"), QStringLiteral(SYMSTUDIO_TWITCH_SCOPES));
	q.addQueryItem(QStringLiteral("device_code"), deviceCode);
	q.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
	QNetworkReply *reply = nam->post(req, q.toString(QUrl::FullyEncoded).toUtf8());
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
		const QString token = o.value(QStringLiteral("access_token")).toString();
		if (!token.isEmpty()) {
			pollTimer->stop();
			accessToken = token;
			refreshToken = o.value(QStringLiteral("refresh_token")).toString();
			saveStr("AccessToken", accessToken);
			saveStr("RefreshToken", refreshToken);
			setStatus(QStringLiteral("Authorized. Fetching account…"));
			fetchUser();
			return;
		}
		const QString msg = o.value(QStringLiteral("message")).toString();
		if (!msg.isEmpty() && !msg.contains(QStringLiteral("authorization_pending")))
			setStatus(QStringLiteral("Login: %1").arg(msg));
	});
}

void SymStudioStreamDock::fetchUser()
{
	QNetworkRequest req(QUrl(QStringLiteral("https://api.twitch.tv/helix/users")));
	req.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());
	req.setRawHeader("Client-Id", clientId.toUtf8());
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const QJsonArray data = QJsonDocument::fromJson(reply->readAll())
						.object().value(QStringLiteral("data")).toArray();
		if (data.isEmpty()) {
			setStatus(QStringLiteral("Could not fetch account (token issue). Try Login again."));
			return;
		}
		const QJsonObject u = data.first().toObject();
		broadcasterId = u.value(QStringLiteral("id")).toString();
		login = u.value(QStringLiteral("display_name")).toString();
		saveStr("BroadcasterId", broadcasterId);
		saveStr("Login", login);
		updateUiState();
		loadChannelInfo();
	});
}

void SymStudioStreamDock::loadChannelInfo(bool allowRefresh)
{
	QUrl url(QStringLiteral("https://api.twitch.tv/helix/channels"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("broadcaster_id"), broadcasterId);
	url.setQuery(q);
	QNetworkRequest req(url);
	req.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());
	req.setRawHeader("Client-Id", clientId.toUtf8());
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply, allowRefresh]() {
		reply->deleteLater();
		const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (code == 401) {
			// Refresh once. If the freshly-refreshed token still 401s, stop here —
			// otherwise refresh→reload→401→refresh spins network calls forever.
			if (allowRefresh)
				tryRefreshThen();
			else
				setStatus(QStringLiteral("Session expired — please Log out and Login again."));
			return;
		}
		const QJsonArray data = QJsonDocument::fromJson(reply->readAll())
						.object().value(QStringLiteral("data")).toArray();
		if (data.isEmpty())
			return;
		const QJsonObject c = data.first().toObject();
		titleEdit->setText(c.value(QStringLiteral("title")).toString());
		categoryEdit->setText(c.value(QStringLiteral("game_name")).toString());
		selectedGameId = c.value(QStringLiteral("game_id")).toString();
	});
}

void SymStudioStreamDock::onSearchCategory()
{
	const QString text = categoryEdit->text().trimmed();
	if (text.isEmpty())
		return;
	QUrl url(QStringLiteral("https://api.twitch.tv/helix/search/categories"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("query"), text);
	url.setQuery(q);
	QNetworkRequest req(url);
	req.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());
	req.setRawHeader("Client-Id", clientId.toUtf8());
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const QJsonArray data = QJsonDocument::fromJson(reply->readAll())
						.object().value(QStringLiteral("data")).toArray();
		categoryResults->clear();
		for (const QJsonValue &v : data) {
			const QJsonObject g = v.toObject();
			categoryResults->addItem(g.value(QStringLiteral("name")).toString(),
						 g.value(QStringLiteral("id")).toString());
		}
		if (categoryResults->count() > 0)
			setStatus(QStringLiteral("Pick a category from the list, then Update."));
		else
			setStatus(QStringLiteral("No categories matched."));
	});
}

void SymStudioStreamDock::onUpdateClicked()
{
	if (categoryResults->currentIndex() >= 0 && !categoryResults->currentData().toString().isEmpty())
		selectedGameId = categoryResults->currentData().toString();

	QUrl url(QStringLiteral("https://api.twitch.tv/helix/channels"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("broadcaster_id"), broadcasterId);
	url.setQuery(q);
	QNetworkRequest req(url);
	req.setRawHeader("Authorization", ("Bearer " + accessToken).toUtf8());
	req.setRawHeader("Client-Id", clientId.toUtf8());
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

	QJsonObject body;
	body.insert(QStringLiteral("title"), titleEdit->text());
	if (!selectedGameId.isEmpty())
		body.insert(QStringLiteral("game_id"), selectedGameId);
	const QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

	QNetworkReply *reply = nam->sendCustomRequest(req, "PATCH", data);
	setStatus(QStringLiteral("Updating…"));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (code == 204 || code == 200)
			setStatus(QStringLiteral("Updated ✓"));
		else if (code == 401)
			setStatus(QStringLiteral("Session expired — log out and log in again."));
		else
			setStatus(QStringLiteral("Update failed (HTTP %1): %2")
					  .arg(code)
					  .arg(QString::fromUtf8(reply->readAll().left(160))));
	});
}

bool SymStudioStreamDock::tryRefreshThen()
{
	if (refreshToken.isEmpty())
		return false;
	QNetworkRequest req(QUrl(QStringLiteral("https://id.twitch.tv/oauth2/token")));
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("client_id"), clientId);
	q.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
	q.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
	QNetworkReply *reply = nam->post(req, q.toString(QUrl::FullyEncoded).toUtf8());
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
		const QString token = o.value(QStringLiteral("access_token")).toString();
		if (token.isEmpty()) {
			setStatus(QStringLiteral("Session expired — please Log out and Login again."));
			return;
		}
		accessToken = token;
		refreshToken = o.value(QStringLiteral("refresh_token")).toString();
		saveStr("AccessToken", accessToken);
		saveStr("RefreshToken", refreshToken);
		setStatus(QStringLiteral("Session refreshed."));
		loadChannelInfo(false); // don't allow this reload to trigger another refresh
	});
	return true;
}
