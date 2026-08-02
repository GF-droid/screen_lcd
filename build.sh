#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLATFORM=""

usage() {
    echo "Usage:"
    echo "  ./build.sh -linux      # x86 Linux PC 模拟"
    echo "  ./build.sh -t113       # T113-S3 交叉编译"
    echo "  ./build.sh -clean      # 清理所有编译输出"
    exit 0
}

case "${1:-}" in
    -linux)  PLATFORM="linux" ;;
    -t113)   PLATFORM="t113" ;;
    -clean)
        echo "Cleaning build directories..."
        rm -rf "$PROJECT_DIR/build"
        echo "Done."
        exit 0
        ;;
    *)       usage ;;
esac

BUILD_DIR="$PROJECT_DIR/build/${PLATFORM}"

echo "========================================"
echo " LVGL Multi-Platform Build"
echo "========================================"
echo " Platform : ${PLATFORM}"
echo " Build    : ${BUILD_DIR}"
echo ""

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$PLATFORM" = "t113" ]; then
    TOOLCHAIN_BIN="$PROJECT_DIR/toolchain-sunxi-glibc-gcc-830/toolchain/bin"
    export STAGING_DIR="$TOOLCHAIN_BIN"
    echo "[1/2] Configuring (cross-compile)..."
    cmake "$PROJECT_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/platform/t113/t113.cmake" \
        -DLV_BUILD_CONF_DIR="$PROJECT_DIR/platform/t113/src/porting"
else
    echo "[1/2] Configuring (native)..."
    cmake "$PROJECT_DIR" \
        -DSIMULATOR_LINUX=linux \
        -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/platform/x86linux/linux.cmake" \
        -DLV_BUILD_CONF_DIR="$PROJECT_DIR/platform/x86linux/src/porting"
fi

echo ""
echo "[2/2] Compiling..."
make -j$(nproc)

echo ""
echo "========================================"
echo " Build Complete"
echo "========================================"
echo "Binary:"
find "$BUILD_DIR" -type f -executable \
    -not -path "*/CMakeFiles/*" \
    -not -path "*/lvgl_build/*" \
    | while read f; do
    echo "  $f"
    file "$f" 2>/dev/null | head -1
done
