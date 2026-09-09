import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'backend.dart';
import 'types.dart';

final ClangConstexprBackend backend = _NativeBackend();

final class _NativeResult extends Struct {
  @Int32()
  external int status;
  @Int32()
  external int kind;
  @Int32()
  external int bitWidth;
  @Int32()
  external int isSigned;
  @Double()
  external double floatingValue;
  @Array(160)
  external Array<Char> integerValue;
  @Array(96)
  external Array<Char> typeName;
  @Array(1024)
  external Array<Char> errorMessage;
}

@Native<Int32 Function(Int32, Pointer<Char>, Uint32, Pointer<_NativeResult>)>(
  assetId: 'package:pcalc_clang_constexpr/pcalc_clang_constexpr',
  symbol: 'pcalc_constexpr_evaluate_language',
)
external int _evaluateNative(
  int language,
  Pointer<Char> expression,
  int expressionLength,
  Pointer<_NativeResult> result,
);

String _readArray(Array<Char> value, int capacity) {
  final units = <int>[];
  for (var index = 0; index < capacity && value[index] != 0; index++) {
    units.add(value[index] & 0xff);
  }
  return utf8.decode(units);
}

final class _NativeBackend implements ClangConstexprBackend {
  @override
  Future<void> initialize({String? moduleUrl, String? wasmUrl}) async {}

  @override
  ClangEvaluationResult evaluate(
    String expression, {
    required ClangExpressionLanguage language,
  }) {
    final encodedLength = utf8.encode(expression).length;
    final encoded = expression.toNativeUtf8();
    final result = calloc<_NativeResult>();
    try {
      _evaluateNative(
        _nativeLanguageCode(language),
        encoded.cast(),
        encodedLength,
        result,
      );
      final native = result.ref;
      if (native.status != 0) {
        throw ClangEvaluationException(_readArray(native.errorMessage, 1024));
      }
      final kind = ClangValueKind.values[native.kind];
      return ClangEvaluationResult(
        kind: kind,
        displayText: kind == ClangValueKind.floating
            ? native.floatingValue.toString()
            : _readArray(native.integerValue, 160),
        typeName: _readArray(native.typeName, 96),
        bitWidth: native.bitWidth,
        isSigned: native.isSigned != 0,
        floatingValue: kind == ClangValueKind.floating
            ? native.floatingValue
            : null,
      );
    } finally {
      calloc.free(result);
      malloc.free(encoded);
    }
  }
}

// Older packaged native libraries understand the generic C/C++ values only.
// Their semantics deliberately track C23 and C++20, so retain ABI compatibility
// for the two defaults while newer libraries support the complete enum.
int _nativeLanguageCode(ClangExpressionLanguage language) => switch (language) {
  ClangExpressionLanguage.c23 => ClangExpressionLanguage.c.index,
  ClangExpressionLanguage.cpp20 => ClangExpressionLanguage.cpp.index,
  _ => language.index,
};
