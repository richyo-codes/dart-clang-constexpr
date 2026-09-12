# dart-clang-constexpr

Evaluate typed C and C++ constant expressions from Dart using Clang. Get the
value, C/C++ type, bit width, and signedness through one API on Linux,
Android arm64, and Flutter web.

Built for pcalc express. Not yet published to pub.dev.

## Quick start

Add the GitHub repository as a dependency in your Flutter project's `pubspec.yaml`:

```yaml
dependencies:
  dart_clang_constexpr:
    git:
      url: https://github.com/richyo-codes/dart-clang-constexpr.git
```

Requires Dart 3.13 or newer. On Linux, install CMake, Ninja, and LLVM/Clang
development libraries; see the [build guide](docs/building.md) for setup.

```dart
import 'package:dart_clang_constexpr/dart_clang_constexpr.dart';

Future<void> main() async {
  await initializeClangConstexpr();

  final result = evaluateClangExpression(
    '(unsigned char)270',
    language: ClangExpressionLanguage.c23,
  );

  print(result.displayText); // 14
  print(result.typeName);    // unsigned char
  print(result.bitWidth);    // 8
  print(result.isSigned);    // false
}
```

Initialization loads the packaged WebAssembly module on web and completes
immediately on native platforms. Invalid or unsupported expressions throw
`ClangEvaluationException`.

## What it supports

Arithmetic, bit operations, comparisons, ternaries, and supported casts, with
Clang's language and type rules. Results distinguish integers, floating-point
values, booleans, and characters.

Choose C99, C11, C17, C23, or C++11/14/17/20/23. The default is C++20.

Inputs are individual expressions with numeric constant results. Invoked C++
lambdas can contain local variables, loops, local types, and recursive calls,
subject to the selected language standard. C++20 immediate (`consteval`) lambdas
are supported. Multiline expressions and comments are accepted.
Top-level declarations, directives, full programs, and runtime evaluation are
not supported; standard-library headers are not supplied.
See [expression semantics](docs/expression-semantics.md) for the detailed limits
and planned target ABI support.

## Platforms

| Platform | Integration |
| --- | --- |
| Linux | Dart's build hook compiles and bundles the native library using your LLVM/Clang installation. |
| Android arm64 | Bundles the checked-in native library; other Android ABIs are not supported yet. |
| Flutter web | Loads the packaged WebAssembly assets after initialization. |

Android's stripped library is about 40.6 MB, so package size is a significant
tradeoff. macOS, Windows, and iOS do not yet have native build-hook support.

## Development

See [building and testing](docs/building.md) for the native CLI and C ABI,
Dart FFI setup, Android and WASM rebuilds, and CI reproduction.

MIT licensed. LLVM and Clang retain their upstream licenses.
