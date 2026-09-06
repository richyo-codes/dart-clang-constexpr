#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
LLVM_PROJECT_DIR=${LLVM_PROJECT_DIR:-/home/ry/code_flutter/llvm-project}
ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-/home/ry/Android/Sdk/ndk/29.0.14033849}
HOST_TOOLS_DIR=${HOST_TOOLS_DIR:-$PROJECT_DIR/build-host-tools}
LLVM_BUILD_DIR=${LLVM_BUILD_DIR:-$PROJECT_DIR/build-android-llvm}
EVALUATOR_BUILD_DIR=${EVALUATOR_BUILD_DIR:-$PROJECT_DIR/build-android-evaluator}

for required_path in \
  "$LLVM_PROJECT_DIR/llvm/CMakeLists.txt" \
  "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  "$HOST_TOOLS_DIR/bin/llvm-tblgen" \
  "$HOST_TOOLS_DIR/bin/clang-tblgen"; do
  if [[ ! -e "$required_path" ]]; then
    echo "Missing required path: $required_path" >&2
    exit 1
  fi
done

cmake -S "$LLVM_PROJECT_DIR/llvm" -B "$LLVM_BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=WebAssembly \
  -DLLVM_TABLEGEN="$HOST_TOOLS_DIR/bin/llvm-tblgen" \
  -DCLANG_TABLEGEN="$HOST_TOOLS_DIR/bin/clang-tblgen" \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_ENABLE_EH=OFF \
  -DLLVM_ENABLE_RTTI=OFF \
  -DLLVM_ENABLE_THREADS=OFF \
  -DLLVM_ENABLE_ZLIB=OFF \
  -DLLVM_ENABLE_ZSTD=OFF \
  -DLLVM_ENABLE_LIBXML2=OFF \
  -DLLVM_ENABLE_LIBEDIT=OFF \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DCLANG_INCLUDE_TESTS=OFF \
  -DLLVM_BUILD_TOOLS=OFF \
  -DCLANG_BUILD_TOOLS=OFF \
  -DLLVM_ENABLE_PIC=ON

cmake --build "$LLVM_BUILD_DIR" --target clangFrontend

cmake -S "$PROJECT_DIR" -B "$EVALUATOR_BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DLLVM_DIR="$LLVM_BUILD_DIR/lib/cmake/llvm" \
  -DClang_DIR="$LLVM_BUILD_DIR/lib/cmake/clang" \
  -DPCALC_CONSTEXPR_BUILD_SHARED=ON

cmake --build "$EVALUATOR_BUILD_DIR" --target pcalc_clang_constexpr

LIBRARY="$EVALUATOR_BUILD_DIR/libpcalc_clang_constexpr.so"
"$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" \
  --strip-unneeded "$LIBRARY"
echo "Built Android arm64 evaluator: $LIBRARY"
