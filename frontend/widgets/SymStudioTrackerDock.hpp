#pragma once

#include <QWidget>
#include <QHash>
#include <QString>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QScrollArea;
class QTimer;
class QNetworkAccessManager;

// SymStudio multi-channel Twitch "Stream Tracker" board (reuses the v2f login).
class SymStudioTrackerDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioTrackerDock(QWidget *parent = nullptr);

private slots:
	void onSave();
	void onPoll();

private:
	struct Stream {
		QString name, game, title, started, thumbUrl;
		int viewers = 0;
	};

	void loadConfig();
	void setStatus(const QString &text);
	void rebuildBoard();
	void fetchThumb(QLabel *label, const QString &urlTemplate);

	QLineEdit *watchEdit = nullptr;
	QPushButton *saveBtn = nullptr;
	QPushButton *refreshBtn = nullptr;
	QLabel *statusLabel = nullptr;
	QLabel *headerLabel = nullptr;
	QScrollArea *scroll = nullptr;
	QWidget *board = nullptr;
	QVBoxLayout *boardLayout = nullptr;
	QTimer *pollTimer = nullptr;
	QNetworkAccessManager *nam = nullptr;

	QStringList channels;
	QHash<QString, Stream> currentStreams; // key = lowercase login
	QHash<QString, int> lastViewers;
};
