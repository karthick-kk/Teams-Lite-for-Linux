#!/bin/bash
# Download and extract CEF binary distribution for Linux x64
set -euo pipefail

CEF_VERSION="145.0.23+g3e7fe1c+chromium-145.0.7632.68"
CEF_DIST="cef_binary_${CEF_VERSION}_linux64"
CEF_URL="https://cef-builds.spotifycdn.com/${CEF_DIST}.tar.bz2"
CEF_DIR="${1:-/tmp/cef}"

if [ -d "$CEF_DIR/include" ]; then
    echo "[cef] Already downloaded at $CEF_DIR"
    exit 0
fi

echo "[cef] Downloading CEF ${CEF_VERSION}..."
mkdir -p "$CEF_DIR"
curl -fSL "$CEF_URL" -o /tmp/cef.tar.bz2

echo "[cef] Extracting..."
tar xjf /tmp/cef.tar.bz2 -C /tmp/
cp -a /tmp/${CEF_DIST}/* "$CEF_DIR/"
rm -f /tmp/cef.tar.bz2

# Build libcef_dll_wrapper
echo "[cef] Building libcef_dll_wrapper..."
mkdir -p "$CEF_DIR/build"
cd "$CEF_DIR/build"
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SANDBOX=OFF
make -j$(nproc) libcef_dll_wrapper

# Symlink resources into Release/ (CEF subprocess expects them there)
ln -sf "$CEF_DIR/Resources/icudtl.dat" "$CEF_DIR/Release/icudtl.dat"
ln -sf "$CEF_DIR/Resources/chrome_100_percent.pak" "$CEF_DIR/Release/chrome_100_percent.pak"
ln -sf "$CEF_DIR/Resources/chrome_200_percent.pak" "$CEF_DIR/Release/chrome_200_percent.pak"
ln -sf "$CEF_DIR/Resources/resources.pak" "$CEF_DIR/Release/resources.pak"
ln -sf "$CEF_DIR/Resources/locales" "$CEF_DIR/Release/locales"

echo "[cef] CEF ready at $CEF_DIR"
