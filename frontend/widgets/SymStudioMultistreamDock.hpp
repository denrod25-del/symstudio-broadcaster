#pragma once

#include <QWidget>
#include <QVector>
#include <QString>

class QVBoxLayout;
class QTimer;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

typedef struct obs_output obs_output_t;
typedef struct obs_service obs_service_t;

// SymStudio Multistream — extra RTMP destinations sharing the main stream's encoders.
class SymStudioMultistreamDock : public QWidget {
	Q_OBJECT

public:
	explicit SymStudioMultistreamDock(QWidget *parent = nullptr);
	~SymStudioMultistreamDock() override;

public slots:
	void onStreamStarted();
	void onStreamStopping();

private slots:
	void addRow();
	void saveDests();
	void pollStatus();

private:
	struct Row {
		int id; // stable, unique per row — used to map an Extra back to its row
		QWidget *w;
		QCheckBox *en;
		QLineEdit *name;
		QComboBox *preset;
		QLineEdit *url;
		QLineEdit *key;
		QLabel *status;
	};
	struct Extra {
		obs_output_t *out;
		obs_service_t *svc;
		int rowId; // Row::id this output belongs to (names may be blank/duplicated)
		int polls;
	};

	void buildRow(const QString &name, const QString &url, const QString &key, bool enabled);
	void loadDests();
	void startExtras();
	void stopExtras();

	QVBoxLayout *rowsLayout = nullptr;
	QVector<Row *> rows;
	QVector<Extra> extras;
	QTimer *pollTimer = nullptr;
	bool streaming = false;
	int rowSeq = 0; // monotonic Row::id source
};
