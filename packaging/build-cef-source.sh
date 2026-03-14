#!/bin/bash
# Build CEF from source with H.264 encoding + PipeWire screen sharing
# Requirements: ~50GB disk, ~16GB RAM, 2-6 hours depending on CPU
set -euo pipefail

CEF_BRANCH="7632"  # CEF 145 / Chromium 145
BUILD_DIR="${1:-/home/kk/dev/cef-build}"
JOBS="${2:-$(nproc)}"

echo "=== CEF Source Build (H.264 encoding + PipeWire) ==="
echo "Branch: ${CEF_BRANCH} (Chromium 145)"
echo "Build dir: ${BUILD_DIR}"
echo "Jobs: ${JOBS}"
echo "Disk needed: ~50GB"
echo ""

# --- Prerequisites ---
echo "[1/7] Checking prerequisites..."

DEPS=(
    git python3 curl lsb-release
    gcc make cmake ninja
    gperf bison flex
    nss alsa-lib libxrandr libxcomposite libxdamage
    cups at-spi2-atk libdrm mesa
    gtk3 dbus libnotify
)

for dep in "${DEPS[@]}"; do
    if ! pacman -Qi "$dep" &>/dev/null; then
        echo "  Installing $dep..."
        sudo pacman -S --noconfirm "$dep"
    fi
done

# --- depot_tools ---
echo "[2/7] Setting up depot_tools..."
if [ ! -d "${BUILD_DIR}/depot_tools" ]; then
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
fi
export PATH="${BUILD_DIR}/depot_tools:${PATH}"

# --- CEF automate script ---
echo "[3/7] Downloading CEF build tools..."
mkdir -p "${BUILD_DIR}/code"
cd "${BUILD_DIR}/code"

if [ ! -f "automate-git.py" ]; then
    curl -fSL "https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py" \
        -o automate-git.py
fi

# --- GN flags ---
echo "[4/7] Configuring build..."
echo "  proprietary_codecs=true  (enable H.264 format support)"
echo "  ffmpeg_branding=Chrome   (full H.264/AAC codec support)"
echo "  rtc_use_h264=true        (OpenH264 for WebRTC H.264 encoding)"
echo "  rtc_use_pipewire=true    (PipeWire screen sharing on Wayland)"
echo ""

# proprietary_codecs=true: enables H.264 format support in the build
# ffmpeg_branding=Chrome: full codec support (H.264/AAC decoding via bundled FFmpeg)
# rtc_use_h264=true: use Cisco's OpenH264 (BSD-licensed) for WebRTC H.264 encoding
# rtc_use_pipewire=true: PipeWire screen capture for native Wayland screen sharing
export GN_DEFINES="is_official_build=true proprietary_codecs=true ffmpeg_branding=Chrome rtc_use_h264=true rtc_use_pipewire=true use_sysroot=false symbol_level=1 is_cfi=false use_thin_lto=false"
export CEF_USE_GN=1

# --- Build ---
echo "[5/7] Starting CEF build (this will take 2-4 hours)..."
echo "  Using ${JOBS} parallel jobs"
echo ""

cd "${BUILD_DIR}"

python3 "${BUILD_DIR}/code/automate-git.py" \
    --download-dir="${BUILD_DIR}/code" \
    --branch="${CEF_BRANCH}" \
    --minimal-distrib \
    --client-distrib \
    --force-clean \
    --x64-build \
    --with-pgo-profiles \
    --jobs="${JOBS}"

# --- Find output ---
echo "[6/7] Locating build output..."
DISTRIB_DIR=$(find "${BUILD_DIR}/code/chromium/src/cef/binary_distrib" -maxdepth 1 -name "cef_binary_*_linux64_minimal" -type d | head -1)

if [ -z "$DISTRIB_DIR" ]; then
    DISTRIB_DIR=$(find "${BUILD_DIR}/code/chromium/src/cef/binary_distrib" -maxdepth 1 -name "cef_binary_*_linux64" -type d | head -1)
fi

if [ -z "$DISTRIB_DIR" ]; then
    echo "ERROR: Could not find CEF distribution output"
    echo "Check ${BUILD_DIR}/code/chromium/src/cef/binary_distrib/"
    exit 1
fi

echo "  Found: ${DISTRIB_DIR}"

# --- Install ---
echo "[7/7] Installing CEF to /tmp/cef..."
rm -rf /tmp/cef
mkdir -p /tmp/cef
cp -a "${DISTRIB_DIR}"/* /tmp/cef/

# Build wrapper
echo "  Building libcef_dll_wrapper..."
mkdir -p /tmp/cef/build
cd /tmp/cef/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SANDBOX=OFF
make -j${JOBS} libcef_dll_wrapper

# Copy resources next to libcef.so (CEF resolves relative to its own location)
cp /tmp/cef/Resources/icudtl.dat /tmp/cef/Release/
cp /tmp/cef/Resources/chrome_100_percent.pak /tmp/cef/Release/
cp /tmp/cef/Resources/chrome_200_percent.pak /tmp/cef/Release/
cp /tmp/cef/Resources/resources.pak /tmp/cef/Release/
cp -r /tmp/cef/Resources/locales /tmp/cef/Release/

echo ""
echo "=== CEF build complete ==="
echo "Installed to: /tmp/cef"
echo ""
echo "Rebuild tfl:"
echo "  cd /home/kk/dev/tfl/build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCEF_ROOT=/tmp/cef && make -j\$(nproc)"
