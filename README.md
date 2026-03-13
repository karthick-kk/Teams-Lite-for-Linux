# tfl — Teams for Linux

Unofficial Microsoft Teams client for Linux. Pure C++, native Wayland, minimal footprint.

Built with [CEF](https://bitbucket.org/chromiumembedded/cef) (Chromium Embedded Framework) — no Electron, no Node.js runtime.

## Features

- **Native Wayland** — runs directly on Wayland via ozone-platform (XWayland fallback)
- **HiDPI** — auto-detects display scale factor
- **Frameless window** — GNOME/KDE provides native window decoration
- **System tray** — minimize to tray, show/quit from tray menu
- **Close-to-tray** — closing the window hides it; quit from tray to exit
- **Cookie persistence** — login session survives restarts (`~/.cache/tfl/`)
- **Window state** — remembers size and position across sessions
- **Single instance** — prevents duplicate processes via flock
- **Desktop notifications** — native libnotify notifications on new messages, click to show window
- **Badge count** — parses unread count from page title, updates tray tooltip
- **Spellcheck** — with right-click suggestions in context menu
- **Screen sharing** — getDisplayMedia permissions auto-granted for Teams
- **Downloads** — auto-saves to `~/Downloads` without dialog
- **Idle override** — injects JS to prevent false "Away" status when window is unfocused
- **External links** — opens non-Teams URLs in default browser via xdg-open
- **Auto-grant permissions** — camera, mic, notifications, clipboard auto-accepted for Teams
- **Suppress prompts** — "save password" and "leave page" dialogs auto-accepted
- **XDG config** — `~/.config/tfl/config` with environment variable overrides
- **Keyboard shortcuts** — F5 reload, Ctrl+R reload, Ctrl+Shift+R hard reload, Ctrl+Q quit, Alt+F4 quit, F12 devtools

## Building

### Prerequisites

```bash
# Arch Linux
sudo pacman -S cmake pkg-config gtk3 libayatana-appindicator libnotify nss alsa-lib curl

# Ubuntu/Debian
sudo apt install build-essential cmake pkg-config libgtk-3-dev \
  libayatana-appindicator3-dev libnotify-dev libnss3-dev libasound2-dev curl

# Fedora
sudo dnf install gcc-c++ cmake pkg-config gtk3-devel \
  libayatana-appindicator-gtk3-devel libnotify-devel nss-devel alsa-lib-devel curl
```

### Download CEF and build

```bash
# Download CEF binary distribution (~300MB) and build the wrapper library
bash packaging/download-cef.sh /tmp/cef

# Build tfl
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCEF_ROOT=/tmp/cef
make -j$(nproc)

# Run
./tfl
```

## Configuration

Config file: `~/.config/tfl/config` (created on first run)

```ini
# Teams URL
# url = https://teams.cloud.microsoft

# Window size
# width = 1280
# height = 800

# Start minimized to tray
# start_minimized = false

# Close to tray (X button hides instead of quitting, Ctrl+Q/Alt+F4 always quit)
# close_to_tray = true

# Enable developer tools (F12)
# dev_tools = false
```

Environment variable overrides (highest priority):

| Variable | Description |
|---|---|
| `TFL_URL` | Teams URL |
| `TFL_WIDTH` | Window width |
| `TFL_HEIGHT` | Window height |
| `TFL_DEV_TOOLS` | Enable F12 devtools (set to any value) |

### Custom CSS

Place a `custom.css` file in `~/.config/tfl/` to inject custom styles into Teams:

```css
/* Example: dark scrollbars */
::-webkit-scrollbar { width: 8px; }
::-webkit-scrollbar-thumb { background: #555; border-radius: 4px; }
```

## Packaging

### GitHub Actions

Packages are built automatically on tag push (`v*`). Produces:
- `.deb` (Ubuntu/Debian)
- `.rpm` (Fedora/RHEL)
- `.pkg.tar.zst` (Arch Linux)

### Local builds with act

Requires [nektos/act](https://github.com/nektos/act) and Docker:

```bash
# Build all packages
bash packaging/build-local.sh all

# Build specific package
bash packaging/build-local.sh deb
bash packaging/build-local.sh rpm
bash packaging/build-local.sh arch
```

## Project structure

```
tfl/
├── CMakeLists.txt                          # Build system
├── src/
│   ├── main.cc                             # Entry point, CEF init
│   ├── app.h/cc                            # Command line flags
│   ├── client.h/cc                         # Browser event handlers
│   ├── window.h/cc                         # Window management
│   ├── config.h/cc                         # Configuration
│   ├── tray.h/cc                           # System tray
│   ├── notifications.h/cc                  # Desktop notifications
│   └── idle.h/cc                           # Idle/visibility override
├── data/
│   ├── tfl.desktop                         # Desktop entry
│   └── dev.tfl.teams-for-linux.appdata.xml # AppStream metadata
├── packaging/
│   ├── download-cef.sh                     # CEF downloader
│   └── build-local.sh                      # Local package builder
└── .github/workflows/
    └── build.yml                           # CI/CD
```

## License

MIT
