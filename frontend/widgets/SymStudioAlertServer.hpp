#pragma once

#include <QObject>
#include <QString>

class QTcpServer;

// Tiny loopback HTTP server that serves an animated alert overlay page (GET /)
// and the current alert as JSON (GET /alert). A CEF browser source renders the
// page; SymStudioAlertsDock calls pushAlert() to fire an alert.
class SymStudioAlertServer : public QObject {
	Q_OBJECT

public:
	explicit SymStudioAlertServer(QObject *parent = nullptr);

	// Binds 127.0.0.1 on the preferred port (28782), falling back to the next
	// few ports if taken. Safe to call once; returns true if now listening.
	bool start();

	bool isListening() const;
	quint16 port() const { return boundPort; }

	// Bump the alert id and store the latest text/type; the overlay page polls
	// /alert and animates when the id increases.
	void pushAlert(const QString &text, const QString &type);

private slots:
	void onNewConnection();

private:
	QString overlayHtml() const;
	QString alertJson() const;

	QTcpServer *server = nullptr;
	quint16 boundPort = 0;
	int alertId = 0;
	QString alertText;
	QString alertType;
};
