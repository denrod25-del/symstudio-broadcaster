# SymStudio

A personal rebrand of [OBS Studio](https://github.com/obsproject/obs-studio), licensed under the
**GNU General Public License v2 (GPLv2)**.

SymStudio **is** OBS Studio's engine with SymStudio branding. All credit for the underlying
software belongs to the OBS Project and its contributors. SymStudio does not use OBS Studio's
trademarks or logos.

## Fork point

- Upstream: `https://github.com/obsproject/obs-studio.git`
- Forked from commit: `f61619ce30665e6a31969518cc64f78733bb7ee5`
- Rebrand changes live on branch: `symstudio-rebrand`

## Build (Windows)

Requirements:
- **Visual Studio 2022** (Build Tools or full IDE) with the **Desktop C++** workload, the
  **C++ ATL** component (`Microsoft.VisualStudio.Component.VC.ATL`), and **Windows SDK 10.0.22621**.
  > Note: OBS's `windows-x64` preset hardcodes the *Visual Studio 17 2022* generator and SDK 22621.
  > VS 2026 alone does **not** satisfy it, and the ATL component is required by several plugins
  > (`frontend-tools`, `obs-qsv11`, `win-dshow` virtualcam).
- **CMake 3.30+** on PATH.
- **Git**.

Steps (PowerShell, from the repo root):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

The first configure downloads prebuilt dependencies (Qt, FFmpeg, CEF, x264, …) automatically.

## Run

The portable build is produced under the build tree:

```
build_x64\rundir\RelWithDebInfo\bin\64bit\SymStudio.exe
```

Run `SymStudio.exe` directly — no installer required.

## What was rebranded (v1)

- Executable name: `obs64.exe` → **`SymStudio.exe`** (`frontend/CMakeLists.txt`)
- Window title + Qt app display name → **SymStudio** (`frontend/widgets/OBSBasic.cpp`,
  `frontend/OBSApp.cpp`)
- Application + window/tray icon → SymStudio icon (`frontend/cmake/windows/obs-studio.ico`,
  `cmake/bundle/windows/obs-studio.ico`, `frontend/forms/images/obs.png`; source asset in
  `assets/symstudio-icon.ico`, regenerable via `assets/make-icon.py`)
- User-facing UI strings → SymStudio (`frontend/data/locale/en-US.ini`)
- About dialog → **SymStudio**, with the attribution *"Based on OBS Studio (GPLv2)"*
  (`frontend/forms/OBSAbout.ui`, `About.Info`)

The internal config directory name (`obs-studio` under `%APPDATA%`) was intentionally left
unchanged, so settings/profiles are preserved.

## Known limitations (deferred past v1)

- **No installer / no code signing** — v1 is a portable run-from-folder build only.
- **Auto-update is not rebranded and will not work.** `frontend/updater/updater.cpp` still
  references `obs64` (it waits for / relaunches `obs64.exe`) and points at OBS's update servers.
  This is irrelevant to the portable build but would need work before any SymStudio release that
  ships updates.
- No custom theme, plugins, or features — v1 is a pure rebrand.

## License

SymStudio is based on OBS Studio and distributed under the GNU GPL v2. See the bundled GPLv2
license text (Help → About → License) and the `AUTHORS` file for OBS contributors.

## Custom features
- **Welcome dock** — a guided Quick-Start dock (frontend/widgets/SymStudioWelcomeDock.*) with quick-action buttons (Add Source, Stream, Record, Settings), an auto-detecting setup checklist, and rotating tips. Toggle from the Docks menu.

- **Midnight Cyan theme** — SymStudio's default dark theme (frontend/data/themes/SymStudio_MidnightCyan.ovt), a glossy 3D Yami variant: navy surfaces, cyan accent, gradient/bevel depth, Bahnschrift headings. Switch themes in Settings -> Appearance.

- **Starter scenes** — one click in the Welcome dock ("Install Starter Scenes") creates the "SymStudio Starter" collection: Starting Soon (countdown text; make it tick via Tools > Scripts > countdown.lua with Text Source 'Countdown'), Live (display capture + styled info bar/webcam frame), Be Right Back, and Ending — all in the Midnight Cyan style. Overlay art ships in data/obs-studio/symstudio-overlays/ (regenerable via assets/make-overlays.py).

- **Twitch chat dock** — a built-in "Chat" dock (frontend/widgets/SymStudioChatDock.*) that connects anonymously (read-only) to Twitch IRC over QTcpSocket. Type a channel, Connect, and watch live chat with colored names. No login required. (Read-only; sending/emotes are future work.)

- **Stream Info dock** — update your Twitch stream title + category from inside SymStudio (frontend/widgets/SymStudioStreamDock.*). One-time setup: register a Twitch app at dev.twitch.tv (enable OAuth Device Code Grant), paste the Client ID, Login (device-code flow, no secret). Uses Helix PATCH /channels. Tokens stored in config.

- **Alerts dock** — on-stream Twitch alerts for subs/resubs/gift subs/raids/cheers, detected over anonymous IRC (no login). Connect to your channel; alerts show in the dock feed and (toggle) on-canvas via a "SymStudio Alert" text source (place it in your scene). "Test alert" verifies the pipeline. (Follows + animated/sound overlays are future work.)

- **Cyberpunk theme** — SymStudio's default dark theme (frontend/data/themes/SymStudio_Cyberpunk.ovt): navy-black surfaces with a neon cyan x magenta cyberpunk duo (magenta hover/accents, cyan->magenta slider, deep-black window backdrop), glossy gradient/bevel, Bahnschrift headings. Midnight Cyan and Yami remain selectable in Settings -> Appearance.

- **Symbolic dock** — embeds the Symbolic desktop browser inside SymStudio (Windows-only, frontend/widgets/SymStudioSymbolicDock.*). Launch && Embed reparents Symbolic's window into the dock (frame stripped); the button also re-attaches if it detaches; closing the dock returns Symbolic to a normal window. Experimental (external-process window embedding); path editable.

- **Stream Tracker dock** — a live multi-channel Twitch dashboard (frontend/widgets/SymStudioTrackerDock.*). Add a watchlist; tiles show live/offline, viewer count with up/down delta, game, title, uptime, and a thumbnail, auto-refreshing every 45s via Helix /streams. Reuses your Stream Info login (log in there first). Live-video tiles are future work.


- **Animated alert overlays** — the Alerts dock can also show real animated, sounded alerts on canvas (tick "Animated overlay (browser)"). SymStudio runs a tiny local server (127.0.0.1:28782) serving an HTML/CSS overlay; it auto-adds a "SymStudio Alert Overlay" browser source to your current scene (position it once). Slide-in/pop/fade + a Web-Audio chime, all restyleable in HTML. Independent of the text on-canvas mode.
