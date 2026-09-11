#!/usr/bin/env bash
set -euo pipefail
target="${1:?Usage: bash scripts/ci_build.sh linux|wasm|android}"
case "$target" in linux|wasm|android) ;; *) exit 2 ;; esac
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"
export LLVM_PROJECT_DIR="${LLVM_PROJECT_DIR:?Set LLVM_PROJECT_DIR}"
export HOST_TOOLS_DIR="${HOST_TOOLS_DIR:-$repo_dir/build-ci-host}"
export BUILD_JOBS="${BUILD_JOBS:-2}"
common=(
  -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
  -DLLVM_ENABLE_PROJECTS=clang -DLLVM_TARGETS_TO_BUILD=WebAssembly
  -DLLVM_ENABLE_EH=OFF -DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_PIC=ON
  -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF
  -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_LIBEDIT=OFF
  -DLLVM_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_TESTS=OFF
  -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF
  -DLLVM_PARALLEL_LINK_JOBS=1
)
cmake -S "$LLVM_PROJECT_DIR/llvm" -B "$HOST_TOOLS_DIR" "${common[@]}"
if [[ "$target" == linux ]]; then
  cmake --build "$HOST_TOOLS_DIR" --target clangFrontend --parallel "$BUILD_JOBS"
  export LLVM_DIR="$HOST_TOOLS_DIR/lib/cmake/llvm"
  export Clang_DIR="$HOST_TOOLS_DIR/lib/cmake/clang"
  cmake -S . -B build-ci-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR="$LLVM_DIR" -DClang_DIR="$Clang_DIR"
  cmake --build build-ci-linux --parallel "$BUILD_JOBS"
  ctest --test-dir build-ci-linux --output-on-failure
  flutter pub get
  dart analyze
  dart test
else
  cmake --build "$HOST_TOOLS_DIR" --target llvm-tblgen clang-tblgen --parallel "$BUILD_JOBS"
  if [[ "$target" == android ]]; then
    bash scripts/build_android_arm64.sh
    test -s build-android-evaluator/libpcalc_clang_constexpr.so
  else
    emcmake cmake -S "$LLVM_PROJECT_DIR/llvm" -B build-ci-wasm-llvm "${common[@]}" \
      -DLLVM_ENABLE_THREADS=OFF \
      -DLLVM_TABLEGEN="$HOST_TOOLS_DIR/bin/llvm-tblgen" \
      -DCLANG_TABLEGEN="$HOST_TOOLS_DIR/bin/clang-tblgen"
    cmake --build build-ci-wasm-llvm --target clangFrontend --parallel "$BUILD_JOBS"
    export LLVM_DIR="$repo_dir/build-ci-wasm-llvm/lib/cmake/llvm"
    export Clang_DIR="$repo_dir/build-ci-wasm-llvm/lib/cmake/clang"
    bash scripts/build_wasm.sh
  fi
fi
