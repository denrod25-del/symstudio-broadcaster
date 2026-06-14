#pragma once

#include <QWidget>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QTimer;

// SymStudio dock that embeds the external "Symbolic" browser window (Windows-only).
class SymStudioSymbolicDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioSymbolicDock(QWidget *parent = nullptr);
	~SymStudioSymbolicDock() override;

protected:
	bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
	void onLaunchClicked();
	void onPollTick();

private:
	void loadConfig();
	void setStatus(const QString &text);
	bool findWindow();
	void embed();
	void detach();

	QLineEdit *pathEdit = nullptr;
	QPushButton *launchBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QVBoxLayout *root = nullptr;
	QWidget *host = nullptr;
	QTimer *pollTimer = nullptr;

	QString exePath;
	qint64 launchedPid = 0;
	int pollLeft = 0;
	void *embeddedHwnd = nullptr; // HWND (void* keeps windows.h out of the header)
	long savedStyle = 0;
};
