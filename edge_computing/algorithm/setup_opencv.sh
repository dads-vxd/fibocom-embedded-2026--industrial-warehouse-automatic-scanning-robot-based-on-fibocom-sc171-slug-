#!/bin/bash
set -e

OPENCV_VERSION="4.10.0"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
THIRD_PARTY_DIR="${SCRIPT_DIR}/third_party"
BUILD_DIR="${THIRD_PARTY_DIR}/opencv_build"
INSTALL_DIR="${BUILD_DIR}/install"
OPENCV_SRC="${BUILD_DIR}/opencv"
CONTRIB_SRC="${BUILD_DIR}/opencv_contrib"

echo "=== Checking system dependencies ==="
# GTK3 for GUI (imshow), other common deps
sudo apt install -y \
    build-essential cmake git pkg-config \
    libgtk-3-dev libavcodec-dev libavformat-dev libswscale-dev \
    libv4l-dev libxvidcore-dev libx264-dev \
    libjpeg-dev libpng-dev libtiff-dev \
    libopenexr-dev libatlas-base-dev \
    libtbb-dev libeigen3-dev \
    python3-dev python3-numpy \
    libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev \
    2>/dev/null || echo "Some system packages may already be installed"

mkdir -p "${THIRD_PARTY_DIR}"

# Helper: clone with tag, try mirrors
clone_with_tag() {
    local name="$1"
    local gitee_url="$2"
    local github_url="$3"
    local dest="$4"
    local tag="$5"

    if [ -d "${dest}/.git" ]; then
        echo "=== ${name} already cloned, checking out ${tag} ==="
        cd "${dest}"
        git fetch --tags 2>/dev/null || true
        git checkout "${tag}" 2>/dev/null || echo "Warning: tag ${tag} not found, using current HEAD"
        cd - > /dev/null
        return 0
    fi

    # Remove stale empty dir
    rm -rf "${dest}"

    echo "=== Cloning ${name} ${tag} ==="
    # Try Gitee mirror first (faster in China), then GitHub
    for url in "${gitee_url}" "${github_url}"; do
        echo "  Trying: ${url}"
        if git clone --depth 1 --branch "${tag}" "${url}" "${dest}" 2>&1; then
            return 0
        fi
        echo "  Failed with --branch, trying full clone + checkout..."
        rm -rf "${dest}"
        if git clone "${url}" "${dest}" 2>/dev/null; then
            cd "${dest}"
            git fetch --tags 2>/dev/null || true
            git checkout "${tag}" 2>/dev/null && cd - > /dev/null && return 0
            cd - > /dev/null
        fi
        rm -rf "${dest}"
    done

    echo "ERROR: Failed to clone ${name} from all mirrors"
    return 1
}

clone_with_tag \
    "OpenCV" \
    "https://gitee.com/mirrors/opencv.git" \
    "https://github.com/opencv/opencv.git" \
    "${OPENCV_SRC}" \
    "${OPENCV_VERSION}"

clone_with_tag \
    "OpenCV Contrib" \
    "https://gitee.com/mirrors/opencv_contrib.git" \
    "https://github.com/opencv/opencv_contrib.git" \
    "${CONTRIB_SRC}" \
    "${OPENCV_VERSION}"

# Configure
echo ""
echo "=== Configuring OpenCV ==="
mkdir -p "${BUILD_DIR}/build"
cd "${BUILD_DIR}/build"

cmake "${OPENCV_SRC}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DOPENCV_EXTRA_MODULES_PATH="${CONTRIB_SRC}/modules" \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_DOCS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_TESTS=OFF \
    -DWITH_GTK=ON \
    -DWITH_OPENGL=ON \
    -DWITH_OPENMP=ON \
    -DWITH_EIGEN=ON \
    -DWITH_GSTREAMER=ON \
    -DWITH_V4L=ON \
    -DOPENCV_ENABLE_NONFREE=OFF

echo ""
echo "=== Building OpenCV (using $(nproc) cores) ==="
make -j$(nproc)

echo ""
echo "=== Installing OpenCV to ${INSTALL_DIR} ==="
make install

echo ""
echo "=============================================="
echo " OpenCV ${OPENCV_VERSION} build complete!"
echo " Installed to: ${INSTALL_DIR}"
echo ""
echo " To build this project with local OpenCV:"
echo "   ./build.sh"
echo "=============================================="
