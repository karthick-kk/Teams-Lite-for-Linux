#!/bin/bash
# Run GitHub Actions workflows locally using nektos/act
# Generates .deb, .rpm, and .pkg.tar.zst packages under build/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/packages"

mkdir -p "$BUILD_DIR"

# Which job to run (default: all)
JOB="${1:-all}"

run_job() {
    local job_name="$1"
    echo "=== Building ${job_name} ==="
    cd "$PROJECT_DIR"
    act workflow_dispatch \
        -j "$job_name" \
        --artifact-server-path "$BUILD_DIR" \
        -W .github/workflows/build.yml \
        --env APP_VERSION=0.1.0 \
        --env CEF_DIR=/tmp/cef
}

case "$JOB" in
    deb)
        run_job build-deb
        ;;
    rpm)
        run_job build-rpm
        ;;
    arch)
        run_job build-arch
        ;;
    all)
        run_job build-deb
        run_job build-rpm
        run_job build-arch
        ;;
    *)
        echo "Usage: $0 [deb|rpm|arch|all]"
        echo "  deb   — Build .deb package (Ubuntu/Debian)"
        echo "  rpm   — Build .rpm package (Fedora/RHEL)"
        echo "  arch  — Build .pkg.tar.zst package (Arch Linux)"
        echo "  all   — Build all packages (default)"
        exit 1
        ;;
esac

echo ""
echo "=== Packages ==="
find "$BUILD_DIR" -type f \( -name "*.deb" -o -name "*.rpm" -o -name "*.pkg.tar.zst" \) -exec ls -lh {} \;
echo ""
echo "Output directory: $BUILD_DIR"
