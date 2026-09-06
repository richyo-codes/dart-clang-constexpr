# pcalc-clang-constexpr

Experimental typed C++ constant-expression evaluator for pcalc express.

Licensed under the MIT License. LLVM and Clang retain their upstream licenses.

The native proof parses an in-memory synthetic `constexpr auto` declaration,
validates its AST against a calculator subset, and evaluates it through
Clang's public `Expr::EvaluateAsConstantExpr` API. It does not use Cling,
CodeGen, LLVM IR, or a JIT.

## Native build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/usr/lib64/llvm22/lib64/cmake/llvm
cmake --build build
ctest --test-dir build --output-on-failure
```

Try an expression:

```bash
./build/pcalc-constexpr '(char)255'
./build/pcalc-constexpr --c '(unsigned char)270'
```

The C API exposes separate C23 and C++20 modes. C++ is the default for backward
compatibility; callers can use `pcalc_constexpr_evaluate_language` to select C.

## Native library

The same C ABI is available as a shared library for Dart FFI and other hosts:

```bash
cmake -S . -B build-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/usr/lib64/llvm22/lib64/cmake/llvm \
  -DClang_DIR=/usr/lib64/llvm22/lib64/cmake/clang
cmake --build build-native
cmake --install build-native --prefix dist
```

Consumers should use only `include/pcalc_constexpr/evaluator.h`. Clang and LLVM
remain implementation details of the library.

## Dart FFI

The repository is also a Dart Native Assets package. On Linux, `dart test` or
a consuming Flutter build invokes CMake and bundles the resulting shared
library. Set `LLVM_DIR` and `Clang_DIR` when CMake cannot discover the desired
installation automatically. Other native targets omit this optional backend
until a matching cross-compiled LLVM/Clang toolchain is provided.

### Android arm64

`scripts/build_android_arm64.sh` cross-builds Clang and the evaluator against
the Android NDK. It requires a matching LLVM source checkout plus host
`llvm-tblgen` and `clang-tblgen` binaries. The defaults match this development
environment and can be overridden with `LLVM_PROJECT_DIR`, `ANDROID_NDK_HOME`,
and `HOST_TOOLS_DIR`.

The initial static-link proof produces a 44 MB stripped arm64 shared library
(about 15.2 MB gzip). It has no LLVM/Clang dynamic dependency, but is too large
to enable by default in a mobile calculator. It is a reproducible baseline for
link-map analysis and feature trimming, not yet a release artifact.

## WASM direction

The evaluator uses a direct `CompilerInstance` with an in-memory source buffer.
It avoids `clangTooling`, the Clang driver, CodeGen, and JIT dependencies. The
WASM build will measure the dead-stripped parser/Sema/AST artifact before any
source extraction is attempted.
