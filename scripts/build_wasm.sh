#!/usr/bin/env bash
set -euo pipefail

# Run after sourcing the Emscripten SDK environment. LLVM_DIR and Clang_DIR
# must reference the cross-built WASM LLVM/Clang CMake packages.
: "${LLVM_DIR:?Set LLVM_DIR to the WASM LLVM CMake package directory.}"
: "${Clang_DIR:?Set Clang_DIR to the WASM Clang CMake package directory.}"

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_dir}/build-wasm"

emcmake cmake -S "${repo_dir}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DLLVM_DIR="${LLVM_DIR}" \
  -DClang_DIR="${Clang_DIR}"
cmake --build "${build_dir}" --target pcalc-constexpr-wasm

install -m 0644 "${build_dir}/pcalc-constexpr-wasm.js" \
  "${repo_dir}/lib/pcalc_clang_constexpr.js"
install -m 0644 "${build_dir}/pcalc-constexpr-wasm.wasm" \
  "${repo_dir}/lib/pcalc_clang_constexpr.wasm"
node "${repo_dir}/tests/wasm_smoke.mjs"
