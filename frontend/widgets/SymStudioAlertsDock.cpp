#include "SymStudioAlertsDock.hpp"
#include "OBSApp.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTcpSocket>
#include <QScrollBar>
#include <QDateTime>
#include <QHash>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

namespace {

QString unescapeTag(const QString &in)
{
	QString out;
	for (int i = 0; i < in.size(); i++) {
		if (in[i] == '\\' && i + 1 < in.size()) {
			const QChar n = in[i + 1];
			if (n == 's') out += ' ';
			else if (n == ':') out += ';';
			else if (n == 'r') out += '\r';
			else if (n == 'n') out += '\n';
			else if (n == '\\') out += '\\';
			else out += n;
			i++;
		} else {
			out += in[i];
		}
	}
	return out;
}

QHash<QString, QString> parseTags(const QString &tags)
{
	QHash<QString, QString> m;
	const QStringList parts = tags.split(';', Qt::SkipEmptyParts);
	for (const QString &kv : parts) {
		int eq = kv.indexOf('=');
		if (eq < 0)
			m.insert(kv, QString());
		else
			m.insert(kv.left(eq), unescapeTag(kv.mid(eq + 1)));
	}
	return m;
}

} // namespace

SymStudioAlertsDock::SymStudioAlertsDock(QWidget *parent) : QWidget(parent)
{
	socket = new QTcpSocket(this);
	connect(socket, &QTcpSocket::connected, this, &SymStudioAlertsDock::onSocketConnected);
	connect(socket, &QTcpSocket::readyRead, this, &SymStudioAlertsDock::onSocketReadyRead);
	connect(socket, &QTcpSocket::disconnected, this, &SymStudioAlertsDock::onSocketDisconnected);

	clearTimer = new QTimer(this);
	clearTimer->setSingleShot(true);
	connect(clearTimer, &QTimer::timeout, this, &SymStudioAlertsDock::clearCanvasText);

	QVBoxLayout *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	QHBoxLayout *top = new QHBoxLayout();
	channelEdit = new QLineEdit(this);
	channelEdit->setPlaceholderText(QStringLiteral("your twitch channel"));
	connectBtn = new QPushButton(QStringLiteral("Connect"), this);
	top->addWidget(channelEdit, 1);
	top->addWidget(connectBtn);
	root->addLayout(top);

	statusLabel = new QLabel(QStringLiteral("Offline"), this);
	statusLabel->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	root->addWidget(statusLabel);

	QHBoxLayout *ctl = new QHBoxLayout();
	canvasCheck = new QCheckBox(QStringLiteral("Show alerts on canvas"), this);
	testBtn = new QPushButton(QStringLiteral("Test alert"), this);
	ctl->addWidget(canvasCheck, 1);
	ctl->addWidget(testBtn);
	root->addLayout(ctl);

	feed = new QTextEdit(this);
	feed->setReadOnly(true);
	feed->setStyleSheet(QStringLiteral("background:#0E121B;border:1px solid #1A2230;border-radius:6px;"));
	root->addWidget(feed, 1);

	setLayout(root);

	connect(connectBtn, &QPushButton::clicked, this, &SymStudioAlertsDock::onConnectClicked);
	connect(testBtn, &QPushButton::clicked, this, &SymStudioAlertsDock::onTestClicked);

	loadConfig();

	connect(canvasCheck, &QCheckBox::toggled, this, [this](bool on) {
		config_set_bool(App()->GetUserConfig(), "SymStudioAlerts", "ShowOnCanvas", on);
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	});
}

void SymStudioAlertsDock::setConnectedState(bool on)
{
	connected = on;
	connectBtn->setText(on ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
}

void SymStudioAlertsDock::saveStr(const char *key, const QString &val)
{
	config_set_string(App()->GetUserConfig(), "SymStudioAlerts", key, val.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

void SymStudioAlertsDock::loadConfig()
{
	const char *ch = config_get_string(App()->GetUserConfig(), "SymStudioAlerts", "Channel");
	if (!ch || !*ch)
		ch = config_get_string(App()->GetUserConfig(), "SymStudioChat", "Channel");
	if (ch && *ch)
		channelEdit->setText(QString::fromUtf8(ch));
	canvasCheck->setChecked(config_get_bool(App()->GetUserConfig(), "SymStudioAlerts", "ShowOnCanvas"));
}

void SymStudioAlertsDock::onConnectClicked()
{
	if (connected || socket->state() != QAbstractSocket::UnconnectedState) {
		socket->abort();
		setConnectedState(false);
		statusLabel->setText(QStringLiteral("Offline"));
		return;
	}
	channel = channelEdit->text().trimmed().toLower();
	if (channel.startsWith('#'))
		channel.remove(0, 1);
	if (channel.isEmpty()) {
		statusLabel->setText(QStringLiteral("Enter your channel name"));
		return;
	}
	saveStr("Channel", channel);
	statusLabel->setText(QStringLiteral("Connecting…"));
	socket->connectToHost(QStringLiteral("irc.chat.twitch.tv"), 6667);
}

void SymStudioAlertsDock::onSocketConnected()
{
	const QString nick =
		QStringLiteral("justinfan%1").arg(10000 + (QDateTime::currentMSecsSinceEpoch() % 800000));
	socket->write("CAP REQ :twitch.tv/tags twitch.tv/commands\r\n");
	socket->write(QStringLiteral("NICK %1\r\n").arg(nick).toUtf8());
	socket->write(QStringLiteral("JOIN #%1\r\n").arg(channel).toUtf8());
	setConnectedState(true);
	statusLabel->setText(QStringLiteral("Listening: #%1").arg(channel));
}

void SymStudioAlertsDock::onSocketReadyRead()
{
	rxBuffer += socket->readAll();
	int idx;
	while ((idx = rxBuffer.indexOf("\r\n")) != -1) {
		QString line = QString::fromUtf8(rxBuffer.left(idx));
		rxBuffer.remove(0, idx + 2);
		if (line.startsWith(QStringLiteral("PING"))) {
			socket->write("PONG :tmi.twitch.tv\r\n");
			continue;
		}
		handleLine(line);
	}
}

void SymStudioAlertsDock::onSocketDisconnected()
{
	setConnectedState(false);
	statusLabel->setText(QStringLiteral("Disconnected"));
}

void SymStudioAlertsDock::handleLine(const QString &lineIn)
{
	QString rest = lineIn;
	QString tagStr;
	if (rest.startsWith('@')) {
		int sp = rest.indexOf(' ');
		if (sp < 0)
			return;
		tagStr = rest.mid(1, sp - 1);
		rest = rest.mid(sp + 1);
	}
	if (rest.startsWith(':')) {
		int sp = rest.indexOf(' ');
		if (sp < 0)
			return;
		rest = rest.mid(sp + 1);
	}
	const QString command = rest.section(' ', 0, 0);
	const QHash<QString, QString> tags = parseTags(tagStr);

	if (command == QStringLiteral("USERNOTICE")) {
		const QString sys = tags.value(QStringLiteral("system-msg"));
		if (!sys.isEmpty())
			addAlert(sys);
	} else if (command == QStringLiteral("PRIVMSG")) {
		const QString bits = tags.value(QStringLiteral("bits"));
		if (!bits.isEmpty() && bits != QStringLiteral("0")) {
			QString name = tags.value(QStringLiteral("display-name"));
			if (name.isEmpty())
				name = QStringLiteral("Someone");
			addAlert(QStringLiteral("%1 cheered %2 bits!").arg(name, bits));
		}
	}
}

void SymStudioAlertsDock::addAlert(const QString &text)
{
	feed->append(QStringLiteral("<b style='color:#00E5FF'>★</b> %1").arg(text.toHtmlEscaped()));
	const int maxBlocks = 100;
	while (feed->document()->blockCount() > maxBlocks) {
		QTextCursor c(feed->document()->begin());
		c.select(QTextCursor::BlockUnderCursor);
		c.removeSelectedText();
		c.deleteChar();
	}
	feed->verticalScrollBar()->setValue(feed->verticalScrollBar()->maximum());

	if (canvasCheck->isChecked())
		updateCanvas(text);
}

void SymStudioAlertsDock::onTestClicked()
{
	addAlert(QStringLiteral("TestUser just subscribed at Tier 1! (test)"));
}

void SymStudioAlertsDock::updateCanvas(const QString &text)
{
	obs_source_t *src = obs_get_source_by_name("SymStudio Alert");
	if (!src) {
		obs_data_t *s = obs_data_create();
		obs_data_t *font = obs_data_create();
		obs_data_set_string(font, "face", "Bahnschrift");
		obs_data_set_int(font, "size", 72);
		obs_data_set_obj(s, "font", font);
		obs_data_release(font);
		obs_data_set_string(s, "text", text.toUtf8().constData());
		src = obs_source_create("text_gdiplus_v3", "SymStudio Alert", s, nullptr);
		if (!src)
			src = obs_source_create("text_gdiplus", "SymStudio Alert", s, nullptr);
		obs_data_release(s);
	} else {
		obs_data_t *s = obs_data_create();
		obs_data_set_string(s, "text", text.toUtf8().constData());
		obs_source_update(src, s);
		obs_data_release(s);
	}
	if (!src)
		return;

	obs_source_t *sceneSource = obs_frontend_get_current_scene();
	if (sceneSource) {
		obs_scene_t *scene = obs_scene_from_source(sceneSource);
		if (scene && !obs_scene_find_source(scene, "SymStudio Alert"))
			obs_scene_add(scene, src);
		obs_source_release(sceneSource);
	}
	obs_source_release(src);

	clearTimer->start(6000);
}

void SymStudioAlertsDock::clearCanvasText()
{
	obs_source_t *src = obs_get_source_by_name("SymStudio Alert");
	if (!src)
		return;
	obs_data_t *s = obs_data_create();
	obs_data_set_string(s, "text", "");
	obs_source_update(src, s);
	obs_data_release(s);
	obs_source_release(src);
}
