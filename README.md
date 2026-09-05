# pcalc-clang-constexpr

Experimental typed C++ constant-expression evaluator for pcalc express.

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

## WASM direction

The evaluator uses a direct `CompilerInstance` with an in-memory source buffer.
It avoids `clangTooling`, the Clang driver, CodeGen, and JIT dependencies. The
WASM build will measure the dead-stripped parser/Sema/AST artifact before any
source extraction is attempted.
