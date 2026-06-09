#pragma once

#include <QWidget>

class OBSBasic;
class QLabel;
class QPushButton;
class QTimer;

// SymStudio Welcome / Quick-Start dock content widget.
class SymStudioWelcomeDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioWelcomeDock(OBSBasic *main, QWidget *parent = nullptr);

private slots:
	void onAddSource();
	void onToggleStream();
	void onToggleRecord();
	void onOpenSettings();
	void refresh();
	void nextTip();

private:
	OBSBasic *main = nullptr;
	QPushButton *streamBtn = nullptr;
	QPushButton *recordBtn = nullptr;
	QTimer *refreshTimer = nullptr;
	QLabel *chkSource = nullptr;
	QLabel *chkAudio = nullptr;
	QLabel *chkKey = nullptr;
	QLabel *chkLive = nullptr;
	bool wentLive = false;
	QLabel *tipLabel = nullptr;
	int tipIndex = 0;
};
