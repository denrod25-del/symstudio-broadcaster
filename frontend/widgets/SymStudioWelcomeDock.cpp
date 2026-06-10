#include "SymStudioWelcomeDock.hpp"
#include "OBSBasic.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QTimer>

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include <utility/platform.hpp>
#include <cstring>
#include <string>

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
		b->setStyleSheet(QStringLiteral(
			"QPushButton{background:#1b1b2e;border:1px solid #00838f;border-radius:6px;"
			"padding:6px;color:#e6f7ff;} QPushButton:hover{border-color:#00E5FF;background:#222338;}"));
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
	QPushButton *starterBtn = makeButton(QStringLiteral("Install Starter Scenes"));
	connect(starterBtn, &QPushButton::clicked, this, &SymStudioWelcomeDock::onInstallStarterScenes);

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

	tipLabel = new QLabel(this);
	tipLabel->setWordWrap(true);
	tipLabel->setStyleSheet(QStringLiteral("color:#A0A8B4;font-style:italic;margin-top:8px;"));
	layout->addWidget(tipLabel);

	QPushButton *nextTipBtn = new QPushButton(QStringLiteral("Next tip"), this);
	nextTipBtn->setFlat(true);
	nextTipBtn->setCursor(Qt::PointingHandCursor);
	connect(nextTipBtn, &QPushButton::clicked, this, &SymStudioWelcomeDock::nextTip);
	layout->addWidget(nextTipBtn);

	layout->addStretch(1);
	setLayout(layout);

	refreshTimer = new QTimer(this);
	refreshTimer->setInterval(1000);
	connect(refreshTimer, &QTimer::timeout, this, &SymStudioWelcomeDock::refresh);
	refreshTimer->start();
	refresh();
	nextTip();
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

void SymStudioWelcomeDock::nextTip()
{
	static const char *tips[] = {
		"Tip: Click the + under Sources to add your webcam, screen, or game.",
		"Tip: Studio Mode (top-right) lets you preview a scene before going live.",
		"Tip: Right-click a source, then Filters to add a chroma key or color correction.",
		"Tip: Set your stream key in Settings, Stream before going live.",
		"Tip: Use scenes to switch layouts - intro, gameplay, 'be right back'.",
	};
	const int count = int(sizeof(tips) / sizeof(tips[0]));
	tipLabel->setText(QString::fromUtf8(tips[tipIndex % count]));
	tipIndex++;
}

static const char *kStarterCollection = "SymStudio Starter";

namespace {

std::string overlayPath(const char *file)
{
	std::string path;
	if (!GetDataFilePath((std::string("symstudio-overlays/") + file).c_str(), path))
		return std::string();
	return path;
}

obs_source_t *createTextSource(const char *name, const char *text, int size)
{
	obs_data_t *s = obs_data_create();
	obs_data_t *fontObj = obs_data_create();
	obs_data_set_string(fontObj, "face", "Bahnschrift");
	obs_data_set_int(fontObj, "size", size);
	obs_data_set_obj(s, "font", fontObj);
	obs_data_release(fontObj);
	obs_data_set_string(s, "text", text);
	obs_source_t *src = obs_source_create("text_gdiplus_v3", name, s, nullptr);
	if (!src)
		src = obs_source_create("text_gdiplus", name, s, nullptr);
	obs_data_release(s);
	return src;
}

obs_sceneitem_t *addImageToScene(obs_scene_t *scene, const char *name, const std::string &file)
{
	if (file.empty())
		return nullptr;
	obs_data_t *s = obs_data_create();
	obs_data_set_string(s, "file", file.c_str());
	obs_source_t *src = obs_source_create("image_source", name, s, nullptr);
	obs_data_release(s);
	if (!src)
		return nullptr;
	obs_sceneitem_t *item = obs_scene_add(scene, src);
	obs_source_release(src);
	return item;
}

} // namespace

void SymStudioWelcomeDock::onInstallStarterScenes()
{
	/* If the collection exists, just switch to it. */
	char **collections = obs_frontend_get_scene_collections();
	bool exists = false;
	for (char **c = collections; c && *c; c++) {
		if (strcmp(*c, kStarterCollection) == 0)
			exists = true;
		bfree(*c);
	}
	bfree(collections);

	if (exists) {
		obs_frontend_set_current_scene_collection(kStarterCollection);
		return;
	}

	if (!obs_frontend_add_scene_collection(kStarterCollection))
		return;

	/* Defer scene building one event-loop turn so the collection switch settles. */
	QTimer::singleShot(0, this, &SymStudioWelcomeDock::buildStarterScenes);
}

void SymStudioWelcomeDock::buildStarterScenes()
{
	/* --- Starting Soon --- */
	obs_scene_t *starting = obs_scene_create("Starting Soon");
	addImageToScene(starting, "Starting Soon Backdrop", overlayPath("bg-starting-soon.png"));
	obs_source_t *count = createTextSource("Countdown", "05:00", 96);
	if (count) {
		obs_sceneitem_t *item = obs_scene_add(starting, count);
		if (item) {
			vec2 pos = {810.0f, 760.0f};
			obs_sceneitem_set_pos(item, &pos);
		}
		obs_source_release(count);
	}
	obs_scene_release(starting);
}
