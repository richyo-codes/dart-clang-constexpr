import 'backend.dart';
import 'types.dart';

final ClangConstexprBackend backend = _UnsupportedBackend();

final class _UnsupportedBackend implements ClangConstexprBackend {
  @override
  Future<void> initialize({String? moduleUrl, String? wasmUrl}) async {
    throw UnsupportedError(
      'The Clang constexpr evaluator is unavailable on this platform.',
    );
  }

  @override
  ClangEvaluationResult evaluate(
    String expression, {
    required ClangExpressionLanguage language,
  }) {
    throw UnsupportedError(
      'The Clang constexpr evaluator is unavailable on this platform.',
    );
  }
}
