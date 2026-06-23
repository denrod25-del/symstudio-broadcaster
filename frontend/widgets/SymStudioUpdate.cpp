#include "SymStudioUpdate.hpp"

#include <QWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <obs.h>

#define SYMSTUDIO_REPO "denrod25-del/symstudio-broadcaster"

// Parse "1.2.3" (ignoring a leading 'v') into a comparable integer tuple.
static bool parseVersion(const QString &in, int out[3])
{
	out[0] = out[1] = out[2] = 0;
	QRegularExpression re(QStringLiteral("(\\d+)\\.(\\d+)\\.(\\d+)"));
	auto m = re.match(in);
	if (!m.hasMatch())
		return false;
	out[0] = m.captured(1).toInt();
	out[1] = m.captured(2).toInt();
	out[2] = m.captured(3).toInt();
	return true;
}

static bool isNewer(const int latest[3], const int current[3])
{
	for (int i = 0; i < 3; i++) {
		if (latest[i] != current[i])
			return latest[i] > current[i];
	}
	return false;
}

SymStudioUpdate::SymStudioUpdate(QWidget *parent) : QObject(parent), parentWidget(parent)
{
	nam = new QNetworkAccessManager(this);
}

void SymStudioUpdate::check(bool manual)
{
	QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/" SYMSTUDIO_REPO "/releases/latest")));
	req.setRawHeader("Accept", "application/vnd.github+json");
	req.setRawHeader("User-Agent", "SymStudio");

	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply, manual]() {
		reply->deleteLater();
		const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (code != 200) {
			if (manual)
				QMessageBox::information(
					parentWidget, QStringLiteral("SymStudio"),
					QStringLiteral("Could not check for updates right now.\n"
						       "(HTTP %1 / err %2: %3)")
						.arg(code)
						.arg((int)reply->error())
						.arg(reply->errorString()));
			return;
		}

		const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
		const QString tag = o.value(QStringLiteral("tag_name")).toString();
		const QString htmlUrl = o.value(QStringLiteral("html_url")).toString();

		int latest[3], current[3];
		const QString currentVer = QString::fromUtf8(obs_get_version_string());
		parseVersion(currentVer, current);

		if (!parseVersion(tag, latest) || !isNewer(latest, current)) {
			if (manual)
				QMessageBox::information(parentWidget, QStringLiteral("SymStudio"),
							 QStringLiteral("You're on the latest version."));
			return;
		}

		QMessageBox box(parentWidget);
		box.setWindowTitle(QStringLiteral("SymStudio"));
		box.setText(QStringLiteral("SymStudio %1 is available.").arg(tag));
		box.setInformativeText(QStringLiteral("You're on %1.").arg(currentVer));
		QPushButton *open = box.addButton(QStringLiteral("Open download page"), QMessageBox::AcceptRole);
		box.addButton(QStringLiteral("Later"), QMessageBox::RejectRole);
		box.exec();
		if (box.clickedButton() == open && !htmlUrl.isEmpty())
			QDesktopServices::openUrl(QUrl(htmlUrl));
	});
}
