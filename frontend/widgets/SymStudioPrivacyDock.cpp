#include "SymStudioPrivacyDock.hpp"
#include "OBSApp.hpp"

#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMetaObject>

#include <cstring>
#include <cstdlib>

#include <obs.h>
#include <obs-hotkey.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <graphics/vec2.h>

static void PanicHotkeyCb(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	QMetaObject::invokeMethod((SymStudioPrivacyDock *)data, "togglePanic", Qt::QueuedConnection);
}

static void PrivacyFrontendEvent(enum obs_frontend_event ev, void *data)
{
	if (ev == OBS_FRONTEND_EVENT_EXIT)
		((SymStudioPrivacyDock *)data)->saveHotkey();
}

SymStudioPrivacyDock::SymStudioPrivacyDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	panicBtn = new QPushButton(QStringLiteral("PANIC — blackout"), this);
	panicBtn->setStyleSheet(QStringLiteral("QPushButton{background:#7A1020;color:#FFE;border:1px solid "
					       "#FF2D5A;border-radius:8px;padding:12px;font-weight:bold;}"));
	root->addWidget(panicBtn);

	armedLabel = new QLabel(this);
	armedLabel->setAlignment(Qt::AlignCenter);
	root->addWidget(armedLabel);

	QLabel *hk = new QLabel(QStringLiteral("Bind a global panic key in Settings → Hotkeys "
					       "(\"Privacy Guard: toggle blackout\")."),
				this);
	hk->setWordWrap(true);
	hk->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	root->addWidget(hk);

	QHBoxLayout *boxRow = new QHBoxLayout();
	addBoxBtn = new QPushButton(QStringLiteral("Add privacy box"), this);
	hideAllBtn = new QPushButton(QStringLiteral("Hide all"), this);
	boxRow->addWidget(addBoxBtn, 1);
	boxRow->addWidget(hideAllBtn);
	root->addLayout(boxRow);

	boxList = new QListWidget(this);
	boxList->setStyleSheet(QStringLiteral("background:#0E121B;border:1px solid #1A2230;border-radius:6px;"));
	root->addWidget(boxList, 1);

	QLabel *note = new QLabel(QStringLiteral("Privacy Guard uses reliable manual controls — it does not "
						 "auto-detect sensitive text. You place the protection."),
				  this);
	note->setWordWrap(true);
	note->setStyleSheet(QStringLiteral("color:#5F6B7C;font-size:11px;"));
	root->addWidget(note);

	setLayout(root);

	connect(panicBtn, &QPushButton::clicked, this, &SymStudioPrivacyDock::togglePanic);
	connect(addBoxBtn, &QPushButton::clicked, this, &SymStudioPrivacyDock::addBox);
	connect(hideAllBtn, &QPushButton::clicked, this, &SymStudioPrivacyDock::hideAllBoxes);
	connect(boxList, &QListWidget::itemChanged, this, [this](QListWidgetItem *lwi) {
		const QString nm = lwi->data(Qt::UserRole).toString();
		const bool vis = lwi->checkState() == Qt::Checked;
		obs_source_t *scnSrc = obs_frontend_get_current_scene();
		if (!scnSrc)
			return;
		obs_scene_t *scene = obs_scene_from_source(scnSrc);
		obs_sceneitem_t *it = scene ? obs_scene_find_source(scene, nm.toUtf8().constData()) : nullptr;
		if (it)
			obs_sceneitem_set_visible(it, vis);
		obs_source_release(scnSrc);
	});

	panicHotkeyId = obs_hotkey_register_frontend("SymStudio.PrivacyBlackout",
						     "Privacy Guard: toggle blackout", PanicHotkeyCb, this);
	loadHotkey();
	obs_frontend_add_event_callback(PrivacyFrontendEvent, this);

	updatePanicUi();
	refreshBoxList();
}

SymStudioPrivacyDock::~SymStudioPrivacyDock()
{
	obs_frontend_remove_event_callback(PrivacyFrontendEvent, this);
	saveHotkey();
	if (panicHotkeyId != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(panicHotkeyId);
	if (coverSource)
		obs_source_release(coverSource);
}

obs_source_t *SymStudioPrivacyDock::ensureCover()
{
	if (coverSource)
		return coverSource;
	uint32_t w = 1920, h = 1080;
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		w = ovi.base_width;
		h = ovi.base_height;
	}
	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "color", 0xFF000000); // opaque black (ARGB)
	obs_data_set_int(s, "width", (int)w);
	obs_data_set_int(s, "height", (int)h);
	coverSource = obs_source_create_private("color_source", "SymStudio Privacy Cover", s);
	obs_data_release(s);
	return coverSource;
}

void SymStudioPrivacyDock::togglePanic()
{
	obs_source_t *scnSrc = obs_frontend_get_current_scene();
	if (!scnSrc)
		return;
	obs_scene_t *scene = obs_scene_from_source(scnSrc);
	obs_source_t *cover = ensureCover();
	obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, obs_source_get_name(cover)) : nullptr;
	if (scene && !item) {
		item = obs_scene_add(scene, cover);
		struct vec2 pos = {0.0f, 0.0f};
		obs_sceneitem_set_pos(item, &pos);
	}
	// Derive the target from THIS scene's cover, not a global flag. Otherwise, after a
	// scene switch, a stale global bool could make PANIC reveal instead of black out —
	// the worst-case failure for a privacy feature. If the current scene isn't covered,
	// pressing PANIC always blacks it out.
	const bool currentlyCovered = item && obs_sceneitem_visible(item);
	const bool target = !currentlyCovered;
	if (item) {
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
		obs_sceneitem_set_visible(item, target);
	}
	blackoutOn = target;
	obs_source_release(scnSrc);
	updatePanicUi();
}

void SymStudioPrivacyDock::updatePanicUi()
{
	if (blackoutOn) {
		armedLabel->setText(QStringLiteral("● BLACKOUT ACTIVE"));
		armedLabel->setStyleSheet(QStringLiteral("color:#FF2D5A;font-weight:bold;"));
		panicBtn->setText(QStringLiteral("Reveal (blackout on)"));
	} else {
		armedLabel->setText(QStringLiteral("output visible"));
		armedLabel->setStyleSheet(QStringLiteral("color:#7E8796;"));
		panicBtn->setText(QStringLiteral("PANIC — blackout"));
	}
}

void SymStudioPrivacyDock::addBox()
{
	obs_source_t *scnSrc = obs_frontend_get_current_scene();
	if (!scnSrc)
		return;
	obs_scene_t *scene = obs_scene_from_source(scnSrc);

	const QString nm = QStringLiteral("Privacy Box %1").arg(++boxSeq);
	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "color", 0xFF000000);
	obs_data_set_int(s, "width", 400);
	obs_data_set_int(s, "height", 120);
	obs_source_t *src = obs_source_create("color_source", nm.toUtf8().constData(), s, nullptr);
	obs_data_release(s);

	if (scene && src) {
		obs_sceneitem_t *item = obs_scene_add(scene, src);
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			struct vec2 p = {(float)((int)ovi.base_width / 2 - 200),
					 (float)((int)ovi.base_height / 2 - 60)};
			obs_sceneitem_set_pos(item, &p);
		}
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	}
	if (src)
		obs_source_release(src);
	obs_source_release(scnSrc);
	refreshBoxList();
}

struct BoxScan {
	QListWidget *list;
	int maxSeq;
};

static bool ScanBoxes(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *bs = (BoxScan *)param;
	obs_source_t *src = obs_sceneitem_get_source(item);
	const char *n = src ? obs_source_get_name(src) : nullptr;
	if (n && strncmp(n, "Privacy Box ", 12) == 0) {
		int seq = atoi(n + 12);
		if (seq > bs->maxSeq)
			bs->maxSeq = seq;
		QListWidgetItem *lwi = new QListWidgetItem(QString::fromUtf8(n));
		lwi->setFlags(lwi->flags() | Qt::ItemIsUserCheckable);
		lwi->setCheckState(obs_sceneitem_visible(item) ? Qt::Checked : Qt::Unchecked);
		lwi->setData(Qt::UserRole, QString::fromUtf8(n));
		bs->list->addItem(lwi);
	}
	return true;
}

void SymStudioPrivacyDock::refreshBoxList()
{
	boxList->blockSignals(true);
	boxList->clear();
	obs_source_t *scnSrc = obs_frontend_get_current_scene();
	if (scnSrc) {
		obs_scene_t *scene = obs_scene_from_source(scnSrc);
		BoxScan bs{boxList, boxSeq};
		if (scene)
			obs_scene_enum_items(scene, ScanBoxes, &bs);
		if (bs.maxSeq > boxSeq)
			boxSeq = bs.maxSeq;
		obs_source_release(scnSrc);
	}
	boxList->blockSignals(false);
}

void SymStudioPrivacyDock::hideAllBoxes()
{
	obs_source_t *scnSrc = obs_frontend_get_current_scene();
	if (!scnSrc)
		return;
	obs_scene_t *scene = obs_scene_from_source(scnSrc);
	if (scene) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *) -> bool {
				obs_source_t *src = obs_sceneitem_get_source(item);
				const char *n = src ? obs_source_get_name(src) : nullptr;
				if (n && strncmp(n, "Privacy Box ", 12) == 0)
					obs_sceneitem_set_visible(item, false);
				return true;
			},
			nullptr);
	}
	obs_source_release(scnSrc);
	refreshBoxList();
}

void SymStudioPrivacyDock::loadHotkey()
{
	const char *j = config_get_string(App()->GetUserConfig(), "SymStudioPrivacy", "PanicHotkeyBindings");
	if (!j || !*j)
		return;
	obs_data_t *d = obs_data_create_from_json(j);
	obs_data_array_t *arr = obs_data_get_array(d, "b");
	if (arr && panicHotkeyId != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_load(panicHotkeyId, arr);
	obs_data_array_release(arr);
	obs_data_release(d);
}

void SymStudioPrivacyDock::saveHotkey()
{
	if (panicHotkeyId == OBS_INVALID_HOTKEY_ID)
		return;
	obs_data_array_t *arr = obs_hotkey_save(panicHotkeyId);
	obs_data_t *d = obs_data_create();
	obs_data_set_array(d, "b", arr);
	config_set_string(App()->GetUserConfig(), "SymStudioPrivacy", "PanicHotkeyBindings", obs_data_get_json(d));
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	obs_data_release(d);
	obs_data_array_release(arr);
}
