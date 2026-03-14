#!/bin/bash
# Run GitHub Actions workflows locally using nektos/act
# Generates .deb, .rpm, and .pkg.tar.zst packages under build/packages/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/packages"

mkdir -p "$BUILD_DIR"

# Which job to run (default: all)
JOB="${1:-all}"

# Map job names to package patterns and container paths
declare -A PKG_PATTERNS=(
    [build-deb]="tfl_*.deb"
    [build-rpm]="tfl-*.rpm"
    [build-arch]="tfl-*.pkg.tar.zst"
)
declare -A PKG_CONTAINER_PATHS=(
    [build-deb]="/home/kk/dev/tfl/build/tfl_0.1.0_amd64.deb"
    [build-rpm]="/home/kk/dev/tfl/build/tfl-0.1.0-1.fc41.x86_64.rpm"
    [build-arch]="/tmp/tfl.pkg.tar.zst"
)
declare -A PKG_CONTAINERS=(
    [build-deb]="ubuntu:24.04"
    [build-rpm]="fedora:41"
    [build-arch]="archlinux:base-devel"
)

run_job() {
    local job_name="$1"
    echo "=== Building ${job_name} ==="
    cd "$PROJECT_DIR"

    # Run act — upload-artifact will fail (no Node.js in container), that's OK
    act workflow_dispatch \
        -j "$job_name" \
        --artifact-server-path "$BUILD_DIR" \
        -W .github/workflows/build.yml \
        --env APP_VERSION=0.1.0 \
        --env CEF_DIR=/tmp/cef || true

    # Extract package from container (act leaves it running/stopped)
    local container_image="${PKG_CONTAINERS[$job_name]}"
    local container_path="${PKG_CONTAINER_PATHS[$job_name]}"
    local container_id
    container_id=$(docker ps -a --filter "ancestor=${container_image}" --format "{{.ID}}" | head -1)

    if [ -n "$container_id" ]; then
        echo "  Extracting package from container ${container_id}..."
        docker cp "${container_id}:${container_path}" "$BUILD_DIR/" 2>/dev/null && \
            echo "  Package extracted to $BUILD_DIR/" || \
            echo "  Warning: could not extract package from container"
    else
        echo "  Warning: no container found for ${container_image}"
    fi
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
