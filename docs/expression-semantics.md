# Expression semantics

[Back to the README](../README.md)

The evaluator parses an in-memory synthetic declaration and asks Clang's public
`Expr::EvaluateAsConstantExpr` API for a numeric constant result. It does not use
Cling, CodeGen, LLVM IR, a JIT, or native runtime execution.

## Language modes

Choose C99, C11, C17, C23, or C++11/14/17/20/23. The original C and C++ API values
remain aliases for C23 and C++20. The default is C++20.

## Expressions and functions

Arithmetic, casts, comparisons, ternaries, bit operations, `sizeof`, and other
expressions are accepted when Clang can evaluate them as constants. Results must
be integers, floating-point values, booleans, or characters; pointers, arrays,
closures, and class objects are not calculator result types.

In C++17 and later, invoked constexpr-capable lambdas can contain local variables,
loops, arrays, local types, member calls, and recursive generic lambdas.
C++20 also supports immediate lambdas:

```cpp
[]() consteval { return 6 * 7; }()
```

For example, this returns 120:

```cpp
[] {
  auto factorial = [](auto self, int n) -> int {
    return n < 2 ? 1 : n * self(self, n - 1);
  };
  return factorial(factorial, 5);
}()
```

The selected language still controls legality. C rejects C++ lambdas; a lambda
call is not a constant expression in C++14. A runtime-only call or invalid
constant operation is rejected, never evaluated by executing native code.
Standard-library headers are not provided. User-defined functions can be local
lambdas or constexpr members of local types; top-level function definitions are
not accepted.

## Input boundary and evaluation limits

The input must remain one expression. A raw Clang lexer checks balanced
parentheses, brackets, and braces, rejects top-level semicolons and preprocessing
tokens (including digraph forms), and handles punctuation in comments and
literals correctly. Newlines, comments, and statements within lambda bodies are
allowed. Clang performs the syntax and constant-expression checks.

The C++ wrapper declares a constexpr result. C uses a const initializer followed
by constant evaluation. Both use an in-memory filesystem without host headers.
Inputs are limited to 65,536 bytes and cannot contain embedded NUL bytes.
Constant evaluation has a 100,000-step budget and Clang's default recursion limit.

The established constant evaluator is explicitly selected on all builds.
With the installed LLVM 22 experimental interpreter, the reproduction
`[] { while (true) {} return 1; }()` failed to terminate despite the step budget.
The expected result is a constant-evaluation error. Upstream tracks the missing
budget enforcement in [issue #165951](https://github.com/llvm/llvm-project/issues/165951)
and [fix #176150](https://github.com/llvm/llvm-project/pull/176150)
(commit `f71e32196667264607974e22d28d3badb2d73b5e`).
Do not re-enable the experimental interpreter until every supported toolchain
has the fix and passes the loop-limit regression. The native suite has a
30-second timeout; run the WASM smoke test under an external timeout as well.

These are evaluator limits, not a complete hostile-input sandbox: parsing and
template instantiation have separate resource costs.

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
