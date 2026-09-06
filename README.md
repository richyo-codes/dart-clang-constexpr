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

The initial `CompilerInstance` proof produced a 44,265,208-byte stripped arm64
shared library (15,255,887 bytes gzip). The direct parser/Sema implementation,
ThinLTO, and wrapper IPO reduce that to 40,601,288 bytes stripped (14,007,134
bytes gzip). It has no LLVM/Clang dynamic dependency, but is still too large to
enable by default in a mobile calculator. Full Sema and AST are now the dominant
size floor; further trimming must reduce their source-level feature set rather
than merely changing linker flags.

## WASM direction

The evaluator constructs Clang's diagnostics, source manager, preprocessor,
AST context, parser, and Sema directly around an in-memory source buffer. It
avoids `CompilerInstance`, serialization/modules/PCH, `clangTooling`, the Clang
driver, CodeGen, and JIT dependencies. The WASM build will measure this
dead-stripped parser/Sema/AST artifact before any source extraction is attempted.
