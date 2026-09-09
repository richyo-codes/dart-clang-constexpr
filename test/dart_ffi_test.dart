import 'package:pcalc_clang_constexpr/pcalc_clang_constexpr.dart';
import 'package:test/test.dart';

void main() {
  test('preserves typed C++ integer results', () {
    final result = evaluateClangExpression('(char)255');
    expect(result.kind, ClangValueKind.character);
    expect(result.displayText, '-1');
    expect(result.bitWidth, 8);
    expect(result.isSigned, isTrue);
  });

  test('selects C language mode', () {
    final result = evaluateClangExpression(
      '(unsigned char)270',
      language: ClangExpressionLanguage.c,
    );
    expect(result.kind, ClangValueKind.character);
    expect(result.displayText, '14');
    expect(result.bitWidth, 8);
    expect(result.isSigned, isFalse);
  });

  test('rejects C++ syntax in C language mode', () {
    expect(
      () => evaluateClangExpression(
        'static_cast<unsigned char>(270)',
        language: ClangExpressionLanguage.c,
      ),
      throwsA(isA<ClangEvaluationException>()),
    );
  });

  test('evaluates basic arithmetic in every selectable language standard', () {
    for (final language in selectableClangExpressionLanguages) {
      final result = evaluateClangExpression('40 + 2', language: language);
      expect(result.kind, ClangValueKind.integer, reason: language.displayName);
      expect(result.displayText, '42', reason: language.displayName);
    }
  });

  test('accepts C++ static casts in each selectable C++ standard', () {
    for (final language in selectableClangExpressionLanguages.where(
      (language) => language.isCxx,
    )) {
      final result = evaluateClangExpression(
        'static_cast<unsigned char>(270)',
        language: language,
      );
      expect(result.displayText, '14', reason: language.displayName);
    }
  });
}
