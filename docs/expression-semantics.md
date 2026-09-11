# Expression semantics

[Back to the README](../README.md)

The evaluator parses an in-memory synthetic `constexpr auto` declaration,
validates its AST against a calculator subset, and evaluates it through
Clang's public `Expr::EvaluateAsConstantExpr` API. It does not use Cling,
CodeGen, LLVM IR, or a JIT.

## Language modes

The C API exposes C99, C11, C17, C23, C++11, C++14, C++17, C++20, and C++23.
The original generic C and C++ values remain ABI-compatible aliases for C23 and
C++20. C++ is the default for backward compatibility; callers can use
`pcalc_constexpr_evaluate_language` to select a language standard.

## Functions and constant evaluation

**The public calculator API does not currently support user-written
`consteval` functions or lambdas.** Selecting C++20 or C++23 changes Clang's
language mode; it does not expand this package's expression allowlist.

In C++20, `consteval` declares an immediate function: ordinary calls must
produce a constant expression. A `constexpr` function can also be used at
runtime when called outside a constant-expression context. `consteval` is a
function specifier, not a general prefix to put before a calculation. See the
[C++ specifier rules](https://eel.is/c++draft/dcl.constexpr) and
[Clang's language support table](https://clang.llvm.org/cxx_status.html).

| Capability | Current package support |
| --- | --- |
| Constant arithmetic, bit operations, comparisons, ternaries, supported casts | Yes, subject to language rules and the AST allowlist |
| User-defined `constexpr` functions and lambdas, including recursion | No |
| C++20 `consteval` functions or immediate lambdas | No |
| C++23 `if consteval` statements | No |
| Standard-library calls such as `std::min` and `std::abs` | No; headers and function calls are not provided |

For C++, the wrapper creates `constexpr auto __pcalc_result = (input);` and
calls `Expr::EvaluateAsConstantExpr`. This requires a constant result but is
not an implementation of the user-facing `consteval` function feature.
The source guard rejects braces, semicolons, directives, and line breaks;
the AST allowlist also excludes lambda and function-call nodes. These limits
apply to native and WASM builds.

For example, this is valid C++20 and would produce 42 in a suitable constant
context, but **is rejected by this package**:

```cpp
[]() consteval { return 6 * 7; }()
```

Enabling it would require expanding the source/AST policy, validating function
bodies and calls, and adding tests for language-version rules, recursion and
evaluation limits. Clang already has the language machinery; package support
must be implemented and tested separately.

## Target ABI modelling

The evaluator is a parser and constant-expression interpreter, not a code
generator. Clang's `LangOptions` and `TargetInfo` determine the language rules
and target ABI used while parsing and evaluating an expression. That means a
hosted Linux, Android, or WASM build can model another target's integer and
data-model rules without cross-compiling or executing generated target code.

Planned calculator target profiles include:

- 32-bit ILP32 targets, such as i386 and ARMv7;
- 64-bit LP64 targets, such as x86-64 and AArch64;
- wasm32;
- a selected big-endian profile, such as PowerPC.

This matters for the width and signedness of implementation-defined types such
as `long`, pointer-sized types, `char`, integer promotions, casts, overflow,
and eventually `sizeof`. It lets a developer check the expression semantics
they would get for a chosen ABI from the same calculator binary.

Endianness has almost no observable effect on the current pure-expression
subset: scalar arithmetic, casts, and bit shifts have the same numeric result.
It becomes useful only when the calculator grows byte-oriented capabilities,
such as packed structs, memory views, serialization helpers, or explicitly
interpreting a byte sequence.

This is intentionally not a substitute for a cross toolchain. A target SDK,
headers/sysroot, and code-generation toolchain are still required to compile or
run a real program, use target platform headers, or validate target runtime
behavior. The planned ABI selector would model Clang's self-contained language and type
semantics only.
