enum ClangExpressionLanguage {
  /// Backward-compatible alias for C23.
  c,

  /// Backward-compatible alias for C++20.
  cpp,
  c99,
  c11,
  c17,
  c23,
  cpp11,
  cpp14,
  cpp17,
  cpp20,
  cpp23;

  bool get isCxx => switch (this) {
    c || c99 || c11 || c17 || c23 => false,
    cpp || cpp11 || cpp14 || cpp17 || cpp20 || cpp23 => true,
  };

  String get displayName => switch (this) {
    c => 'C (C23 default)',
    cpp => 'C++ (C++20 default)',
    c99 => 'C99',
    c11 => 'C11',
    c17 => 'C17',
    c23 => 'C23',
    cpp11 => 'C++11',
    cpp14 => 'C++14',
    cpp17 => 'C++17',
    cpp20 => 'C++20',
    cpp23 => 'C++23',
  };
}

const selectableClangExpressionLanguages = <ClangExpressionLanguage>[
  ClangExpressionLanguage.c99,
  ClangExpressionLanguage.c11,
  ClangExpressionLanguage.c17,
  ClangExpressionLanguage.c23,
  ClangExpressionLanguage.cpp11,
  ClangExpressionLanguage.cpp14,
  ClangExpressionLanguage.cpp17,
  ClangExpressionLanguage.cpp20,
  ClangExpressionLanguage.cpp23,
];

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
