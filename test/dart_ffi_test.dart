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
}
