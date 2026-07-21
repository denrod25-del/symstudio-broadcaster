#pragma once

#include <QWidget>

class QPushButton;
class QLabel;
class QListWidget;

typedef struct obs_source obs_source_t;

// SymStudio Privacy Guard — panic blackout hotkey + opaque privacy boxes.
class SymStudioPrivacyDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioPrivacyDock(QWidget *parent = nullptr);
	~SymStudioPrivacyDock() override;

	void saveHotkey();

public slots:
	void togglePanic();

private slots:
	void addBox();
	void hideAllBoxes();
	void refreshBoxList();

private:
	obs_source_t *ensureCover();
	void updatePanicUi();
	void loadHotkey();

	QPushButton *panicBtn = nullptr;
	QLabel *armedLabel = nullptr;
	QPushButton *addBoxBtn = nullptr;
	QPushButton *hideAllBtn = nullptr;
	QListWidget *boxList = nullptr;

	obs_source_t *coverSource = nullptr;      // private, ephemeral full-canvas cover
	unsigned long long panicHotkeyId = ~0ULL; // obs_hotkey_id; ~0 == OBS_INVALID_HOTKEY_ID
	bool blackoutOn = false;
	int boxSeq = 0;
};
