#!/usr/bin/env bash
# Build script for the audiochecksum fooyin plugin.
#
# Prerequisites:
#   fooyin must be installed (or built) with -DINSTALL_HEADERS=ON so that
#   find_package(Fooyin) can locate the headers and cmake config files.
#
#   Install fooyin with headers:
#     cmake -S /path/to/fooyin -G Ninja -B /tmp/fooyin-build \
#           -DINSTALL_HEADERS=ON -DCMAKE_INSTALL_PREFIX=/usr/local
#     cmake --build /tmp/fooyin-build
#     cmake --install /tmp/fooyin-build
#
# Usage:
#   ./build.sh                        # auto-detects fooyin install prefix
#   ./build.sh --prefix /usr/local    # explicit fooyin install prefix
#   ./build.sh --install              # also install plugin after building
#   ./build.sh --debug                # Debug build instead of Release

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="Release"
FOOYIN_PREFIX=""
DO_INSTALL=false
# Build directory used when building fooyin from the submodule
FOOYIN_SUBMODULE_BUILD="${REPO_ROOT}/fooyin/build"

# ---- Argument parsing ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            FOOYIN_PREFIX="$2"
            shift 2
            ;;
        --install)
            DO_INSTALL=true
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            echo "Removing build directory: ${BUILD_DIR}"
            rm -rf "${BUILD_DIR}"
            shift
            ;;
        --help|-h)
            sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# ---- Locate fooyin cmake config ----
CMAKE_EXTRA_ARGS=()

if [[ -n "${FOOYIN_PREFIX}" ]]; then
    CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${FOOYIN_PREFIX}")
else
    # 1. Try building from the fooyin submodule if present
    FOOYIN_SUBMODULE="${REPO_ROOT}/fooyin"
    if [[ -f "${FOOYIN_SUBMODULE}/CMakeLists.txt" ]]; then
        echo "Found fooyin submodule at: ${FOOYIN_SUBMODULE}"
        if [[ ! -f "${FOOYIN_SUBMODULE_BUILD}/FooyinConfig.cmake" ]] && \
           [[ ! -d "${FOOYIN_SUBMODULE_BUILD}/lib/cmake/fooyin" ]]; then
            echo "Building fooyin from submodule (this may take a while)..."
            cmake -S "${FOOYIN_SUBMODULE}" \
                  -B "${FOOYIN_SUBMODULE_BUILD}" \
                  -G Ninja \
                  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
                  -DINSTALL_HEADERS=ON \
                  -DBUILD_PLUGINS=OFF \
                  -DBUILD_TRANSLATIONS=OFF \
                  -DCMAKE_INSTALL_PREFIX="${FOOYIN_SUBMODULE_BUILD}/install"
            cmake --build "${FOOYIN_SUBMODULE_BUILD}" -- -j"$(nproc)"
            cmake --install "${FOOYIN_SUBMODULE_BUILD}" \
                  --prefix "${FOOYIN_SUBMODULE_BUILD}/install"
        fi
        CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${FOOYIN_SUBMODULE_BUILD}/install")
        echo "Using fooyin prefix: ${FOOYIN_SUBMODULE_BUILD}/install"
    else
        # 2. Fall back to common system install prefixes
        for try_prefix in /usr/local /usr; do
            if [[ -d "${try_prefix}/lib/cmake/fooyin" ]]; then
                CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${try_prefix}")
                echo "Found fooyin cmake config at: ${try_prefix}/lib/cmake/fooyin"
                break
            fi
        done
    fi
fi

# ---- Configure ----
echo "Configuring (${BUILD_TYPE})..."
cmake -S "${SCRIPT_DIR}" \
      -B "${BUILD_DIR}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      "${CMAKE_EXTRA_ARGS[@]}"

# ---- Build ----
echo "Building..."
cmake --build "${BUILD_DIR}" -- -j"$(nproc)"

echo ""
echo "Build complete. Plugin library is in:"
find "${BUILD_DIR}" -name "*.so" -o -name "*.dylib" -o -name "*.dll" 2>/dev/null | head -5

# ---- Install (optional) ----
if [[ "${DO_INSTALL}" == true ]]; then
    echo ""
    echo "Installing plugin..."
    cmake --install "${BUILD_DIR}"
    echo "Done."
fi

echo ""
echo "To load in fooyin, copy the .so to your plugin directory:"
echo "  ~/.local/lib/fooyin/plugins/   (user install)"
echo "  /usr/local/lib/fooyin/plugins/ (system install)"
