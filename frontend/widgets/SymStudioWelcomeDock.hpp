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

private:
	OBSBasic *main = nullptr;
};
