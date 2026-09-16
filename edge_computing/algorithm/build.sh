#!/bin/bash
set -e

echo "=== Installing system dependencies ==="
sudo apt update
sudo apt install -y \
    cmake g++ pkg-config \
    libzbar-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libgtk-3-dev

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
THIRD_PARTY="${SCRIPT_DIR}/third_party"

LOCAL_OPENCV="${THIRD_PARTY}/opencv_build/install"

# --- ONNX Runtime: find any onnxruntime* directory ---
LOCAL_ORT=$(ls -d "${THIRD_PARTY}"/onnxruntime* 2>/dev/null | head -1)
if [ -z "$LOCAL_ORT" ]; then
    echo "ERROR: No onnxruntime directory found in third_party/"
    echo "Please download ONNX Runtime and extract to third_party/onnxruntime/"
    echo "  mkdir -p third_party"
    echo "  wget https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-aarch64-1.20.1.tgz"
    echo "  tar xzf onnxruntime-linux-aarch64-1.20.1.tgz"
    echo "  mv onnxruntime-linux-aarch64-1.20.1 third_party/onnxruntime"
    exit 1
fi

# --- OpenCV ---
if [ -d "$LOCAL_OPENCV" ]; then
    echo "Using local OpenCV: $LOCAL_OPENCV"
else
    echo ""
    echo "=== Local OpenCV not found, running setup_opencv.sh ==="
    bash "${SCRIPT_DIR}/setup_opencv.sh"
fi

echo ""
echo "=== Building project ==="
mkdir -p build
cd build
cmake -DCMAKE_PREFIX_PATH="${LOCAL_OPENCV}:${LOCAL_ORT}" ..
make -j$(nproc)

echo ""
echo "=== Done ==="
echo "Run:  ./build/barcode_detect --camera"
