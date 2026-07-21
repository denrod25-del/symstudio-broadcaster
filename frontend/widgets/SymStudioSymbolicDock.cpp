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
	DWORD pid;       // for enumByPid: the process id to match
	QString wantExe; // for enumByExe: lowercased exe basename to match (e.g. "symbolic.exe")
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
// Fallback when we have no tracked pid: identify Symbolic by its *process executable*,
// not by window class + title. "Chrome_WidgetWin_1" is the class for every Chromium /
// Electron window (Chrome, Edge, VS Code, Slack, Discord…), and a title "contains
// Symbolic" match could reparent any of them — even VS Code editing this file. Matching
// the owning process's exe name guarantees we only ever grab the real Symbolic window.
BOOL CALLBACK enumByExe(HWND hwnd, LPARAM lp)
{
	auto *ctx = reinterpret_cast<FindCtx *>(lp);
	if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
		return TRUE;
	if (GetWindowTextLengthW(hwnd) == 0)
		return TRUE;
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (!pid)
		return TRUE;
	HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!proc)
		return TRUE;
	wchar_t path[MAX_PATH] = {0};
	DWORD sz = MAX_PATH;
	bool match = false;
	if (QueryFullProcessImageNameW(proc, 0, path, &sz)) {
		const QString base = QFileInfo(QString::fromWCharArray(path)).fileName().toLower();
		match = (base == ctx->wantExe);
	}
	CloseHandle(proc);
	if (match) {
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
		FindCtx ctx{(DWORD)launchedPid, QString(), nullptr};
		EnumWindows(enumByPid, reinterpret_cast<LPARAM>(&ctx));
		h = ctx.result;
	}
	if (!h) {
		const QString wantExe = QFileInfo(exePath).fileName().toLower();
		if (!wantExe.isEmpty()) {
			FindCtx ctx{0, wantExe, nullptr};
			EnumWindows(enumByExe, reinterpret_cast<LPARAM>(&ctx));
			h = ctx.result;
		}
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
		if (IsWindow(hwnd))
			MoveWindow(hwnd, 0, 0, host->width(), host->height(), TRUE);
		else
			embeddedHwnd = nullptr; // Symbolic closed — stop touching a dead handle
	}
#endif
	return QWidget::eventFilter(obj, e);
}

void SymStudioSymbolicDock::detach()
{
#ifdef _WIN32
	HWND hwnd = reinterpret_cast<HWND>(embeddedHwnd);
	if (hwnd) {
		// Only restore a window that still exists — the handle may be stale (and its
		// numeric value recycled) if Symbolic exited while embedded.
		if (IsWindow(hwnd)) {
			SetParent(hwnd, nullptr);
			SetWindowLongPtrW(hwnd, GWL_STYLE, (LONG_PTR)savedStyle);
			SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
				     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED |
					     SWP_SHOWWINDOW);
		}
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
