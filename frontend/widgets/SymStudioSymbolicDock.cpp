#include "SymStudioSymbolicDock.hpp"
#include "OBSApp.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QProcess>
#include <QFileInfo>
#include <QEvent>

#include <util/config-file.h>

#ifdef _WIN32
#include <windows.h>

namespace {
struct FindCtx {
	DWORD pid; // 0 = ignore pid, use class/title fallback
	HWND result;
};
BOOL CALLBACK enumByPid(HWND hwnd, LPARAM lp)
{
	auto *ctx = reinterpret_cast<FindCtx *>(lp);
	if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
		return TRUE;
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == ctx->pid && GetWindowTextLengthW(hwnd) > 0) {
		ctx->result = hwnd;
		return FALSE;
	}
	return TRUE;
}
BOOL CALLBACK enumByTitle(HWND hwnd, LPARAM lp)
{
	auto *ctx = reinterpret_cast<FindCtx *>(lp);
	if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
		return TRUE;
	wchar_t cls[128] = {0};
	wchar_t title[256] = {0};
	GetClassNameW(hwnd, cls, 128);
	GetWindowTextW(hwnd, title, 256);
	const QString c = QString::fromWCharArray(cls);
	const QString t = QString::fromWCharArray(title);
	if (c == QStringLiteral("Chrome_WidgetWin_1") &&
	    t.contains(QStringLiteral("Symbolic"), Qt::CaseInsensitive)) {
		ctx->result = hwnd;
		return FALSE;
	}
	return TRUE;
}
} // namespace
#endif

SymStudioSymbolicDock::SymStudioSymbolicDock(QWidget *parent) : QWidget(parent)
{
	pollTimer = new QTimer(this);
	connect(pollTimer, &QTimer::timeout, this, &SymStudioSymbolicDock::onPollTick);

	root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	QHBoxLayout *top = new QHBoxLayout();
	pathEdit = new QLineEdit(this);
	pathEdit->setPlaceholderText(QStringLiteral("path to Symbolic.exe"));
	launchBtn = new QPushButton(QStringLiteral("Launch && Embed"), this);
	top->addWidget(pathEdit, 1);
	top->addWidget(launchBtn);
	root->addLayout(top);

	statusLabel = new QLabel(this);
	statusLabel->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	root->addWidget(statusLabel);

	host = new QWidget(this);
	host->setAttribute(Qt::WA_NativeWindow);
	host->setMinimumSize(200, 150);
	host->installEventFilter(this);
	root->addWidget(host, 1);

	setLayout(root);

	connect(launchBtn, &QPushButton::clicked, this, &SymStudioSymbolicDock::onLaunchClicked);

	loadConfig();
#ifndef _WIN32
	launchBtn->setEnabled(false);
	setStatus(QStringLiteral("Symbolic embedding is Windows-only."));
#else
	setStatus(QStringLiteral("Click Launch && Embed."));
#endif
}

SymStudioSymbolicDock::~SymStudioSymbolicDock()
{
	detach();
}

void SymStudioSymbolicDock::setStatus(const QString &text)
{
	statusLabel->setText(text);
}

void SymStudioSymbolicDock::loadConfig()
{
	const char *p = config_get_string(App()->GetUserConfig(), "SymStudioSymbolic", "ExePath");
	if (p && *p) {
		exePath = QString::fromUtf8(p);
	} else {
		exePath = qEnvironmentVariable("USERPROFILE") +
			  QStringLiteral("\\AppData\\Local\\Programs\\Symbolic\\Symbolic.exe");
	}
	pathEdit->setText(exePath);
}

bool SymStudioSymbolicDock::findWindow()
{
#ifdef _WIN32
	HWND h = nullptr;
	if (launchedPid) {
		FindCtx ctx{(DWORD)launchedPid, nullptr};
		EnumWindows(enumByPid, reinterpret_cast<LPARAM>(&ctx));
		h = ctx.result;
	}
	if (!h) {
		FindCtx ctx{0, nullptr};
		EnumWindows(enumByTitle, reinterpret_cast<LPARAM>(&ctx));
		h = ctx.result;
	}
	if (h) {
		embeddedHwnd = reinterpret_cast<void *>(h);
		return true;
	}
#endif
	return false;
}

void SymStudioSymbolicDock::embed()
{
#ifdef _WIN32
	HWND hwnd = reinterpret_cast<HWND>(embeddedHwnd);
	if (!hwnd)
		return;
	savedStyle = (long)GetWindowLongPtrW(hwnd, GWL_STYLE);
	LONG_PTR s = savedStyle;
	s &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
	s |= WS_CHILD;
	SetWindowLongPtrW(hwnd, GWL_STYLE, s);
	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	/* manual reparent into the native host widget — Windows routes input to the
	 * child natively (fixes clicks/typing) and we control geometry ourselves. */
	SetParent(hwnd, reinterpret_cast<HWND>(host->winId()));
	MoveWindow(hwnd, 0, 0, host->width(), host->height(), TRUE);
	ShowWindow(hwnd, SW_SHOW);
	SetFocus(hwnd);
	setStatus(QStringLiteral("Embedded."));
#endif
}

bool SymStudioSymbolicDock::eventFilter(QObject *obj, QEvent *e)
{
#ifdef _WIN32
	if (obj == host && e->type() == QEvent::Resize && embeddedHwnd) {
		HWND hwnd = reinterpret_cast<HWND>(embeddedHwnd);
		MoveWindow(hwnd, 0, 0, host->width(), host->height(), TRUE);
	}
#endif
	return QWidget::eventFilter(obj, e);
}

void SymStudioSymbolicDock::detach()
{
#ifdef _WIN32
	HWND hwnd = reinterpret_cast<HWND>(embeddedHwnd);
	if (hwnd) {
		SetParent(hwnd, nullptr);
		SetWindowLongPtrW(hwnd, GWL_STYLE, (LONG_PTR)savedStyle);
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		embeddedHwnd = nullptr;
	}
#endif
}

void SymStudioSymbolicDock::onLaunchClicked()
{
#ifdef _WIN32
	exePath = pathEdit->text().trimmed();
	config_set_string(App()->GetUserConfig(), "SymStudioSymbolic", "ExePath", exePath.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);

	detach();
	launchedPid = 0;

	if (findWindow()) {
		embed();
		return;
	}
	if (!QFileInfo::exists(exePath)) {
		setStatus(QStringLiteral("Symbolic.exe not found at that path."));
		return;
	}
	QProcess::startDetached(exePath, {}, QFileInfo(exePath).absolutePath(), &launchedPid);
	setStatus(QStringLiteral("Launching Symbolic…"));
	pollLeft = 30; // 30 * 500ms = 15s
	pollTimer->start(500);
#endif
}

void SymStudioSymbolicDock::onPollTick()
{
#ifdef _WIN32
	if (findWindow()) {
		pollTimer->stop();
		embed();
		return;
	}
	if (--pollLeft <= 0) {
		pollTimer->stop();
		setStatus(QStringLiteral("Couldn't find Symbolic's window — click Launch && Embed to re-try."));
	}
#endif
}
