#pragma once

#include <QObject>
#include <QString>

class QWidget;
class QNetworkAccessManager;

// Lightweight update check: GETs the latest GitHub release, compares its tag to
// the running version, and (if newer) shows a non-blocking dialog linking to the
// release page. No auto-install, no OBS servers.
class SymStudioUpdate : public QObject {
	Q_OBJECT

public:
	explicit SymStudioUpdate(QWidget *parent);

	// manual = user clicked Help > Check for Updates (show "up to date" too).
	// When false (timed/launch check) stay silent unless an update exists.
	void check(bool manual);

private:
	QWidget *parentWidget = nullptr;
	QNetworkAccessManager *nam = nullptr;
};
