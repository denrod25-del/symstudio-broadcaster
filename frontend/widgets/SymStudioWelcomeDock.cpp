#include "SymStudioWelcomeDock.hpp"
#include "OBSBasic.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QTimer>

#include <obs.h>
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

	QLabel *chkHeader = new QLabel(QStringLiteral("Setup checklist"), this);
	chkHeader->setStyleSheet(QStringLiteral("font-weight:bold;margin-top:8px;"));
	layout->addWidget(chkHeader);

	auto makeCheck = [&](const QString &text) {
		QLabel *l = new QLabel(QStringLiteral("[ ]  ") + text, this);
		layout->addWidget(l);
		return l;
	};
	chkSource = makeCheck(QStringLiteral("Add your first source"));
	chkAudio = makeCheck(QStringLiteral("Set up audio"));
	chkKey = makeCheck(QStringLiteral("Add your stream key"));
	chkLive = makeCheck(QStringLiteral("Go live!"));

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

namespace {

bool currentSceneHasSource()
{
	obs_source_t *sceneSource = obs_frontend_get_current_scene();
	if (!sceneSource)
		return false;
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	bool has = false;
	if (scene) {
		auto cb = [](obs_scene_t *, obs_sceneitem_t *, void *param) {
			*static_cast<bool *>(param) = true;
			return false; // stop after first item
		};
		obs_scene_enum_items(scene, cb, &has);
	}
	obs_source_release(sceneSource);
	return has;
}

bool anyAudioSourceExists()
{
	bool found = false;
	auto cb = [](void *param, obs_source_t *src) {
		if ((obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO) != 0)
			*static_cast<bool *>(param) = true;
		return true;
	};
	obs_enum_sources(cb, &found);
	return found;
}

bool streamKeySet()
{
	obs_service_t *service = obs_frontend_get_streaming_service();
	if (!service)
		return false;
	obs_data_t *settings = obs_service_get_settings(service);
	const char *key = settings ? obs_data_get_string(settings, "key") : nullptr;
	bool set = key && *key;
	if (settings)
		obs_data_release(settings);
	return set;
}

} // namespace

void SymStudioWelcomeDock::refresh()
{
	const bool streaming = obs_frontend_streaming_active();
	const bool recording = obs_frontend_recording_active();

	streamBtn->setText(streaming ? QStringLiteral("Stop Streaming") : QStringLiteral("Start Streaming"));
	streamBtn->setStyleSheet(streaming ? QStringLiteral("color:#FF5046;font-weight:bold;") : QString());
	recordBtn->setText(recording ? QStringLiteral("Stop Recording") : QStringLiteral("Start Recording"));
	recordBtn->setStyleSheet(recording ? QStringLiteral("color:#FF5046;font-weight:bold;") : QString());

	if (streaming)
		wentLive = true;

	auto setCheck = [](QLabel *l, const QString &text, bool done) {
		l->setText((done ? QStringLiteral("[x]  ") : QStringLiteral("[ ]  ")) + text);
		l->setStyleSheet(done ? QStringLiteral("color:#00E5FF;") : QString());
	};
	setCheck(chkSource, QStringLiteral("Add your first source"), currentSceneHasSource());
	setCheck(chkAudio, QStringLiteral("Set up audio"), anyAudioSourceExists());
	setCheck(chkKey, QStringLiteral("Add your stream key"), streamKeySet());
	setCheck(chkLive, QStringLiteral("Go live!"), wentLive || streaming);
}
