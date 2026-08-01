# Teams Lite for Linux [TFL]

Unofficial Microsoft Teams client for Linux. Pure C++, native Wayland, minimal footprint.

Built with [CEF](https://bitbucket.org/chromiumembedded/cef) (Chromium Embedded Framework) — no Electron, no Node.js runtime.

## Features

- **Native Wayland** — runs directly on Wayland via ozone-platform (XWayland fallback)
- **HiDPI** — auto-detects display scale factor
- **Frameless window** — GNOME/KDE provides native window decoration
- **H.264 video calls** — OpenH264 encoding for Teams video calls (runtime download, royalty-free)
- **Screen sharing** — PipeWire capture via XDG Desktop Portal (native Wayland)
- **System tray** — minimize to tray, show/quit from tray menu
- **Close-to-tray** — closing the window hides it; quit from tray to exit
- **Cookie persistence** — login session survives restarts (`~/.cache/tfl/`)
- **Window state** — remembers size and position across sessions
- **Single instance** — prevents duplicate processes via flock
- **Desktop notifications** — native libnotify notifications on new messages, click to show window
- **Badge count** — parses unread count from page title, updates tray tooltip
- **Downloads** — auto-saves to `~/Downloads` without dialog
- **Idle override** — injects JS to prevent false "Away" status when window is unfocused
- **Theming** — built-in themes (Yaru Dark, Catppuccin Mocha) via tray menu or config
- **VAAPI** — hardware video decode/encode (toggleable via tray or config)
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

CEF 151 (Chromium 151.0.7922.47) with H.264 encoding and PipeWire screen sharing
is downloaded from the [TFL GitHub releases](https://github.com/karthick-kk/Teams-Lite-for-Linux/releases):

```bash
# Download pre-built CEF binary (~300MB) and build the wrapper library
bash packaging/download-cef.sh /tmp/cef

# Build tfl
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCEF_ROOT=/tmp/cef
make -j$(nproc)

# Run
./tfl
```

### Building CEF from source

The pre-built binary above includes H.264 encoding and PipeWire. To build CEF from
source (e.g. for a different Chromium version or custom flags):

```bash
bash packaging/build-cef-source.sh
```

Requires ~50GB disk, ~16GB RAM, 2-6 hours depending on CPU.

## Configuration

Config file: `~/.config/tfl/config` (created on first run)

```ini
# Teams URL
# url = https://teams.microsoft.com/v2/

# Window size and position
# width = 1280
# height = 800
# x = (centered)
# y = (centered)

# Start minimized to tray
# start_minimized = false

# Close to tray (X button hides instead of quitting, Ctrl+Q/Alt+F4 always quit)
# close_to_tray = true

# Enable developer tools (F12)
# dev_tools = false

# Theme: none, yaru-dark, catppuccin-mocha
# theme = none

# Hardware video decode (VAAPI)
# vaapi = true

# Screen share audio (enable only if you have no mic echo)
# screen_share_audio = false

# Idle timeout (seconds before "Away" override kicks in)
# idle_timeout = 300
```

Environment variable overrides (highest priority):

| Variable | Description |
|---|---|
| `TFL_URL` | Teams URL |
| `TFL_WIDTH` | Window width |
| `TFL_HEIGHT` | Window height |
| `TFL_DEV_TOOLS` | Enable F12 devtools (set to any value) |
| `TFL_THEME` | Theme name (none, yaru-dark, catppuccin-mocha) |
| `TFL_VAAPI` | Enable VAAPI hardware video decode (true/false) |
| `TFL_SCREEN_SHARE_AUDIO` | Enable screen share audio (true/false) |
| `TFL_IDLE_TIMEOUT` | Idle timeout in seconds |

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
- `.tar.gz` (portable binary)

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
│   ├── theme.h/cc                          # CSS theme injection
│   ├── idle.h/cc                           # Idle/visibility override
│   └── openh264.h/cc                       # Runtime OpenH264 downloader
├── data/
│   ├── tfl.desktop                         # Desktop entry
│   ├── tfl.svg                             # App icon
│   ├── dev.tfl.teams-for-linux.appdata.xml # AppStream metadata
│   └── themes/                             # CSS themes (yaru-dark, catppuccin-mocha)
├── packaging/
│   ├── download-cef.sh                     # CEF binary downloader (GitHub release)
│   ├── build-cef-source.sh                 # CEF source build (H.264 + PipeWire)
│   ├── build-local.sh                      # Local package builder (via act)
│   └── PKGBUILD                            # Arch Linux package
└── .github/workflows/
    └── build.yml                           # CI/CD (deb/rpm/arch/binary)
```

## License

MIT

## Similar Project / Inspired from

https://github.com/IsmaelMartinez/teams-for-linux