#include "SymStudioChatDock.hpp"
#include "OBSApp.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTcpSocket>
#include <QScrollBar>
#include <QDateTime>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>

#include <util/config-file.h>

SymStudioChatDock::SymStudioChatDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	QHBoxLayout *top = new QHBoxLayout();
	channelEdit = new QLineEdit(this);
	channelEdit->setPlaceholderText(QStringLiteral("twitch channel"));
	connectBtn = new QPushButton(QStringLiteral("Connect"), this);
	connectBtn->setCursor(Qt::PointingHandCursor);
	top->addWidget(channelEdit, 1);
	top->addWidget(connectBtn);
	root->addLayout(top);

	statusLabel = new QLabel(QStringLiteral("Offline"), this);
	statusLabel->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	root->addWidget(statusLabel);

	chatView = new QTextEdit(this);
	chatView->setReadOnly(true);
	chatView->setStyleSheet(QStringLiteral("background:#0E121B;border:1px solid #1A2230;border-radius:6px;"));
	root->addWidget(chatView, 1);

	setLayout(root);

	socket = new QTcpSocket(this);
	connect(socket, &QTcpSocket::connected, this, &SymStudioChatDock::onSocketConnected);
	connect(socket, &QTcpSocket::readyRead, this, &SymStudioChatDock::onSocketReadyRead);
	connect(socket, &QTcpSocket::disconnected, this, &SymStudioChatDock::onSocketDisconnected);

	connect(connectBtn, &QPushButton::clicked, this, &SymStudioChatDock::onConnectClicked);

	const char *saved = config_get_string(App()->GetUserConfig(), "SymStudioChat", "Channel");
	if (saved && *saved)
		channelEdit->setText(QString::fromUtf8(saved));
}

SymStudioChatDock::~SymStudioChatDock()
{
	// On shutdown the socket is destroyed as a child of this dock, which emits
	// disconnected() and would invoke onSocketDisconnected() against an
	// already-torn-down widget tree (crash). Sever the connections first.
	if (socket) {
		socket->disconnect(this);
		socket->abort();
	}
}

void SymStudioChatDock::setConnectedState(bool on)
{
	connected = on;
	connectBtn->setText(on ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
}

void SymStudioChatDock::appendSystem(const QString &text)
{
	chatView->append(QStringLiteral("<i style='color:#7E8796'>%1</i>").arg(text.toHtmlEscaped()));
}

void SymStudioChatDock::appendMessage(const QString &name, const QString &color, const QString &msg)
{
	const QString html = QStringLiteral("<b style='color:%1'>%2</b>: %3")
				     .arg(color.toHtmlEscaped(), name.toHtmlEscaped(), msg.toHtmlEscaped());
	chatView->append(html);

	/* trim to the most recent ~500 lines */
	const int maxBlocks = 500;
	while (chatView->document()->blockCount() > maxBlocks) {
		QTextCursor c(chatView->document()->begin());
		c.select(QTextCursor::BlockUnderCursor);
		c.removeSelectedText();
		c.deleteChar(); // remove the leftover newline
	}

	chatView->verticalScrollBar()->setValue(chatView->verticalScrollBar()->maximum());
}

void SymStudioChatDock::handleLine(const QString &lineIn)
{
	QString line = lineIn;
	QString tags;
	if (line.startsWith('@')) {
		int sp = line.indexOf(' ');
		if (sp < 0)
			return;
		tags = line.mid(1, sp - 1);
		line = line.mid(sp + 1);
	}
	if (!line.startsWith(':'))
		return;
	int sp2 = line.indexOf(' ');
	if (sp2 < 0)
		return;
	const QString prefix = line.mid(1, sp2 - 1); // nick!user@host
	QString rest = line.mid(sp2 + 1);            // "PRIVMSG #chan :msg"
	if (!rest.startsWith(QStringLiteral("PRIVMSG")))
		return;
	int colon = rest.indexOf(QStringLiteral(" :"));
	if (colon < 0)
		return;
	const QString msg = rest.mid(colon + 2);

	QString name = prefix.section('!', 0, 0);
	QString color = QStringLiteral("#00E5FF");
	const QStringList kvs = tags.split(';', Qt::SkipEmptyParts);
	for (const QString &kv : kvs) {
		int eq = kv.indexOf('=');
		if (eq < 0)
			continue;
		const QString k = kv.left(eq);
		const QString v = kv.mid(eq + 1);
		if (k == QStringLiteral("display-name") && !v.isEmpty())
			name = v;
		else if (k == QStringLiteral("color") && !v.isEmpty())
			color = v;
	}
	appendMessage(name, color, msg);
}

void SymStudioChatDock::onConnectClicked()
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
		statusLabel->setText(QStringLiteral("Enter a channel name"));
		return;
	}

	config_set_string(App()->GetUserConfig(), "SymStudioChat", "Channel", channel.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);

	statusLabel->setText(QStringLiteral("Connecting…"));
	socket->connectToHost(QStringLiteral("irc.chat.twitch.tv"), 6667);
}

void SymStudioChatDock::onSocketConnected()
{
	const QString nick =
		QStringLiteral("justinfan%1").arg(10000 + (QDateTime::currentMSecsSinceEpoch() % 800000));
	socket->write("CAP REQ :twitch.tv/tags\r\n");
	socket->write(QStringLiteral("NICK %1\r\n").arg(nick).toUtf8());
	socket->write(QStringLiteral("JOIN #%1\r\n").arg(channel).toUtf8());
	setConnectedState(true);
	statusLabel->setText(QStringLiteral("Connected: #%1").arg(channel));
	appendSystem(QStringLiteral("Connected to #%1").arg(channel));
}

void SymStudioChatDock::onSocketReadyRead()
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

void SymStudioChatDock::onSocketDisconnected()
{
	setConnectedState(false);
	statusLabel->setText(QStringLiteral("Disconnected"));
	appendSystem(QStringLiteral("Disconnected"));
}
