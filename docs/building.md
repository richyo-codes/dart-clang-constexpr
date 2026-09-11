# Building and testing

[Back to the README](../README.md)

## Prerequisites

Native builds require CMake 3.20 or newer, a C++17 compiler, and LLVM/Clang
libraries and development headers. The examples below use a local LLVM 22
installation; adjust the paths for your system. Dart integration uses Ninja
and requires Dart 3.13 or newer and the Flutter SDK for the package's test
dependencies.

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
installation automatically. Android arm64-v8a builds bundle the checked-in,
prebuilt evaluator library; other Android ABIs omit this optional backend until
matching artifacts are provided.

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

## WebAssembly

The evaluator constructs Clang's diagnostics, source manager, preprocessor,
AST context, parser, and Sema directly around an in-memory source buffer. It
avoids `CompilerInstance`, serialization/modules/PCH, `clangTooling`, the Clang
driver, CodeGen, and JIT dependencies.

The package includes a prebuilt Emscripten module plus a browser-safe Dart
backend. On web, call and await `initializeClangConstexpr()` before calling
`evaluateClangExpression`; native platforms complete initialization immediately.
The public result API is the same on web and native targets.

After sourcing Emscripten and setting `LLVM_DIR` and `Clang_DIR` to the
cross-built CMake packages, rebuild and smoke-test the packaged assets with:

```bash
scripts/build_wasm.sh
```

The module is intentionally packaged as a Flutter asset so a consuming Flutter
web build serves the JavaScript loader and its sibling `.wasm` file together.
The browser loader resolves the WASM URL relative to itself, avoiding a hardcoded
application base URL.

## Continuous integration

The GitHub workflow in `.github/workflows/verify.yml` runs on pushes, pull
requests, and manual dispatch. It invokes `scripts/ci_build.sh`:

- Linux builds LLVM/Clang and the evaluator shared library from source, runs
  CTest with assertions enabled, then runs Dart analysis and FFI tests.
- WASM builds host TableGen tools, cross-builds LLVM/Clang using Emscripten,
  rebuilds the packaged evaluator module, and runs the Node smoke test.
- Android builds host TableGen tools and cross-builds LLVM/Clang and the arm64
  evaluator shared library with NDK r29. This checks compilation/linking only;
  it does not execute Android tests or replace the checked-in Android library.

LLVM and Flutter revisions and the Emscripten version are recorded in each
workflow. The Flutter revision supplies a Dart SDK compatible with this package's constraint.
CI fetches toolchain sources outside the package so Dart analysis does not scan
an embedded Flutter checkout.

Jobs use Ubuntu 24.04, two build workers, and a six-hour timeout. Cold LLVM
builds are expensive; the workflow currently rebuilds without a compiler cache. No release is published and no
checked-in binary is automatically updated.

For local reproduction, install the applicable tools, set LLVM_PROJECT_DIR to
the pinned LLVM source checkout, and run `bash scripts/ci_build.sh linux`,
`wasm`, or `android`. For WASM first source emsdk_env.sh; for Android set
ANDROID_NDK_HOME. BUILD_JOBS defaults to 2.

## Package naming compatibility

The Dart package is `dart_clang_constexpr` (formerly `pcalc_clang_constexpr`).
The native C ABI symbols, CMake target, and packaged binary filenames retain
their existing names for compatibility. The browser loader resolves assets
under the new Dart package name.
