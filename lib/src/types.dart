enum ClangExpressionLanguage { c, cpp }

enum ClangValueKind { error, integer, floating, boolean, character }

final class ClangEvaluationResult {
  const ClangEvaluationResult({
    required this.kind,
    required this.displayText,
    required this.typeName,
    required this.bitWidth,
    required this.isSigned,
    this.floatingValue,
  });

  final ClangValueKind kind;
  final String displayText;
  final String typeName;
  final int bitWidth;
  final bool isSigned;
  final double? floatingValue;
}

final class ClangEvaluationException implements Exception {
  const ClangEvaluationException(this.message);

  final String message;

  @override
  String toString() => message;
}
