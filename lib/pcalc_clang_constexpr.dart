import 'src/backend_stub.dart'
    if (dart.library.ffi) 'src/backend_native.dart'
    if (dart.library.js_interop) 'src/backend_web.dart';
import 'src/types.dart';

export 'src/types.dart';

/// Initializes the evaluator for the active platform.
///
/// Native platforms complete immediately. On web this loads the packaged
/// Emscripten module and must be awaited before evaluating expressions.
Future<void> initializeClangConstexpr({String? moduleUrl, String? wasmUrl}) =>
    backend.initialize(moduleUrl: moduleUrl, wasmUrl: wasmUrl);

/// Evaluates a typed C or C++ constant expression.
ClangEvaluationResult evaluateClangExpression(
  String expression, {
  ClangExpressionLanguage language = ClangExpressionLanguage.cpp,
}) => backend.evaluate(expression, language: language);
