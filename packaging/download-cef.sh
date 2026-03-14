#!/bin/bash
# Download pre-built CEF binary distribution with H.264 encoding + PipeWire
# Source: GitHub Release (custom build from CEF branch 7632 / Chromium 145)
set -euo pipefail

CEF_CHROMIUM_VERSION="145.0.7632.160"
CEF_TARBALL="cef_binary_${CEF_CHROMIUM_VERSION}_linux64_h264_pipewire.tar.gz"
CEF_URL="https://github.com/karthick-kk/Teams-Lite-for-Linux/releases/download/cef-${CEF_CHROMIUM_VERSION}/${CEF_TARBALL}"
CEF_DIR="${1:-/tmp/cef}"

if [ -d "$CEF_DIR/include" ]; then
    echo "[cef] Already downloaded at $CEF_DIR"
    exit 0
fi

echo "[cef] Downloading CEF ${CEF_CHROMIUM_VERSION} (H.264 + PipeWire)..."
mkdir -p "$CEF_DIR"
curl -fSL "$CEF_URL" -o /tmp/cef.tar.gz

echo "[cef] Extracting..."
tar xzf /tmp/cef.tar.gz -C /tmp/
cp -a /tmp/cef_binary/* "$CEF_DIR/"
rm -rf /tmp/cef.tar.gz /tmp/cef_binary

# Build libcef_dll_wrapper
echo "[cef] Building libcef_dll_wrapper..."
mkdir -p "$CEF_DIR/build"
cd "$CEF_DIR/build"
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SANDBOX=OFF
make -j$(nproc) libcef_dll_wrapper

# Copy resources into Release/ (CEF resolves relative to libcef.so)
cp "$CEF_DIR/Resources/icudtl.dat" "$CEF_DIR/Release/"
cp "$CEF_DIR/Resources/chrome_100_percent.pak" "$CEF_DIR/Release/"
cp "$CEF_DIR/Resources/chrome_200_percent.pak" "$CEF_DIR/Release/"
cp "$CEF_DIR/Resources/resources.pak" "$CEF_DIR/Release/"
cp -r "$CEF_DIR/Resources/locales" "$CEF_DIR/Release/"

echo "[cef] CEF ready at $CEF_DIR (H.264 encoding + PipeWire screen sharing)"
