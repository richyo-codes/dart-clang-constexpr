import 'types.dart';

abstract interface class ClangConstexprBackend {
  Future<void> initialize({String? moduleUrl, String? wasmUrl});

  ClangEvaluationResult evaluate(
    String expression, {
    required ClangExpressionLanguage language,
  });
}
