#include "SymStudioWelcomeDock.hpp"
#include "OBSBasic.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QTimer>

#include <obs-frontend-api.h>

SymStudioWelcomeDock::SymStudioWelcomeDock(OBSBasic *main_, QWidget *parent) : QWidget(parent), main(main_)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setSpacing(10);

	QLabel *title = new QLabel(QStringLiteral("Welcome to SymStudio"), this);
	title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #00E5FF;"));
	layout->addWidget(title);

	auto makeButton = [&](const QString &text) {
		QPushButton *b = new QPushButton(text, this);
		b->setMinimumHeight(40);
		b->setCursor(Qt::PointingHandCursor);
		layout->addWidget(b);
		return b;
	};

	QPushButton *addSrcBtn = makeButton(QStringLiteral("Add Source"));
	streamBtn = makeButton(QStringLiteral("Start Streaming"));
	recordBtn = makeButton(QStringLiteral("Start Recording"));
	QPushButton *settingsBtn = makeButton(QStringLiteral("Settings"));

	connect(addSrcBtn, &QPushButton::clicked, this, &SymStudioWelcomeDock::onAddSource);
	connect(streamBtn, &QPushButton::clicked, this, &SymStudioWelcomeDock::onToggleStream);
	connect(recordBtn, &QPushButton::clicked, this, &SymStudioWelcomeDock::onToggleRecord);
	connect(settingsBtn, &QPushButton::clicked, this, &SymStudioWelcomeDock::onOpenSettings);

	layout->addStretch(1);
	setLayout(layout);

	refreshTimer = new QTimer(this);
	refreshTimer->setInterval(1000);
	connect(refreshTimer, &QTimer::timeout, this, &SymStudioWelcomeDock::refresh);
	refreshTimer->start();
	refresh();
}

void SymStudioWelcomeDock::onAddSource()
{
	// "Add Source" is a private slot on the main window; trigger it by name.
	QMetaObject::invokeMethod(main, "on_actionAddSource_triggered", Qt::QueuedConnection);
}

void SymStudioWelcomeDock::onOpenSettings()
{
	QMetaObject::invokeMethod(main, "on_action_Settings_triggered", Qt::QueuedConnection);
}

void SymStudioWelcomeDock::onToggleStream()
{
	if (obs_frontend_streaming_active())
		obs_frontend_streaming_stop();
	else
		obs_frontend_streaming_start();
}

void SymStudioWelcomeDock::onToggleRecord()
{
	if (obs_frontend_recording_active())
		obs_frontend_recording_stop();
	else
		obs_frontend_recording_start();
}

void SymStudioWelcomeDock::refresh()
{
	const bool streaming = obs_frontend_streaming_active();
	const bool recording = obs_frontend_recording_active();

	streamBtn->setText(streaming ? QStringLiteral("Stop Streaming") : QStringLiteral("Start Streaming"));
	streamBtn->setStyleSheet(streaming ? QStringLiteral("color:#FF5046;font-weight:bold;") : QString());
	recordBtn->setText(recording ? QStringLiteral("Stop Recording") : QStringLiteral("Start Recording"));
	recordBtn->setStyleSheet(recording ? QStringLiteral("color:#FF5046;font-weight:bold;") : QString());
}
