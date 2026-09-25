#!/usr/bin/env bash
# Build script for Google ML Kit Barhopper Android Shim & Protobuf Options
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Building Google ML Kit Linux Port ==="

# 1. Detect Architecture
ARCH="$(uname -m)"
echo "[+] Detected architecture: $ARCH"

if [ "$ARCH" = "x86_64" ]; then
    TARGET_DIR="$SCRIPT_DIR/lib/x86_64"
    CXX="${CXX:-g++}"
elif [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    TARGET_DIR="$SCRIPT_DIR/lib/arm64-v8a"
    CXX="${CXX:-g++}"
else
    echo "[-] Unsupported architecture: $ARCH"
    exit 1
fi

mkdir -p "$TARGET_DIR"

# 2. Compile Bionic-to-Glibc Shim Library
echo "[+] Compiling Bionic translation shim into $TARGET_DIR/libandroid_shim.so..."
$CXX -shared -fPIC -O3 \
    -I"$SCRIPT_DIR/shim" \
    "$SCRIPT_DIR/shim/android_log_shim.cpp" \
    "$SCRIPT_DIR/shim/android_graphics_shim.cpp" \
    "$SCRIPT_DIR/shim/jni_mock.cpp" \
    "$SCRIPT_DIR/shim/got_hook.cpp" \
    -o "$TARGET_DIR/libandroid_shim.so" \
    -ldl

# Create Bionic compatibility symlinks
(cd "$TARGET_DIR" && ln -sf libandroid_shim.so liblog.so && ln -sf libandroid_shim.so libjnigraphics.so)
echo "[+] Built $TARGET_DIR/libandroid_shim.so and symlinks successfully."

# 3. Optional Cross-compilation for AArch64 (Jetson Orin Nano) if cross compiler is present
if [ "$ARCH" = "x86_64" ] && command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
    echo "[+] Found aarch64-linux-gnu-g++, cross-compiling for Jetson Orin Nano..."
    ARM64_DIR="$SCRIPT_DIR/lib/arm64-v8a"
    mkdir -p "$ARM64_DIR"
    aarch64-linux-gnu-g++ -shared -fPIC -O3 \
        -I"$SCRIPT_DIR/shim" \
        "$SCRIPT_DIR/shim/android_log_shim.cpp" \
        "$SCRIPT_DIR/shim/android_graphics_shim.cpp" \
        "$SCRIPT_DIR/shim/jni_mock.cpp" \
        "$SCRIPT_DIR/shim/got_hook.cpp" \
        -o "$ARM64_DIR/libandroid_shim.so" \
        -ldl
    (cd "$ARM64_DIR" && ln -sf libandroid_shim.so liblog.so && ln -sf libandroid_shim.so libjnigraphics.so)
    echo "[+] Cross-compiled Jetson Orin Nano shim: $ARM64_DIR/libandroid_shim.so"
fi

# 4. Generate Official Options Protobuf if needed
if [ ! -f "$REPO_ROOT/models/barhopper_options_official.bin" ]; then
    echo "[+] Synthesizing official barhopper options protobuf..."
    python3 "$SCRIPT_DIR/tools/generate_options_proto.py"
fi

echo "=== Build Complete! ==="
