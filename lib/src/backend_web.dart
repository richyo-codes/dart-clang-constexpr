import 'dart:js_interop';
import 'dart:js_interop_unsafe';

import 'backend.dart';
import 'types.dart';

final ClangConstexprBackend backend = _WebBackend();

final class _WebBackend implements ClangConstexprBackend {
  static const _defaultLoaderUrl =
      './assets/packages/dart_clang_constexpr/lib/pcalc_clang_constexpr_loader.js';

  _ClangWasmModule? _module;
  Future<void>? _initializing;

  @override
  Future<void> initialize({String? moduleUrl, String? wasmUrl}) {
    if (_module != null) return Future.value();
    return _initializing ??= _load(moduleUrl: moduleUrl, wasmUrl: wasmUrl);
  }

  Future<void> _load({String? moduleUrl, String? wasmUrl}) async {
    final options = JSObject();
    if (wasmUrl != null) options['wasmUrl'] = wasmUrl.toJS;
    try {
      final loader = _ClangWasmLoader(
        await importModule((moduleUrl ?? _defaultLoaderUrl).toJS).toDart,
      );
      _module = await loader.initializePcalcClangConstexpr(options).toDart;
    } catch (_) {
      _initializing = null;
      rethrow;
    }
  }

  @override
  ClangEvaluationResult evaluate(
    String expression, {
    required ClangExpressionLanguage language,
  }) {
    final module = _module;
    if (module == null) {
      throw StateError(
        'Clang constexpr web backend is not initialized. '
        'Call and await initializeClangConstexpr() first.',
      );
    }
    final result = module.evaluate(language.index, expression);
    if (result.status != 0) throw ClangEvaluationException(result.errorMessage);
    final kind = ClangValueKind.values[result.kind];
    return ClangEvaluationResult(
      kind: kind,
      displayText: result.displayText,
      typeName: result.typeName,
      bitWidth: result.bitWidth,
      isSigned: result.isSigned,
      floatingValue: kind == ClangValueKind.floating
          ? result.floatingValue
          : null,
    );
  }
}

extension type _ClangWasmLoader(JSObject _) implements JSObject {
  external JSPromise<_ClangWasmModule> initializePcalcClangConstexpr(
    JSObject options,
  );
}

extension type _ClangWasmModule(JSObject _) implements JSObject {
  external _ClangWasmResult evaluate(int language, String expression);
}

extension type _ClangWasmResult(JSObject _) implements JSObject {
  external int get status;
  external int get kind;
  external int get bitWidth;
  external bool get isSigned;
  external double get floatingValue;
  external String get displayText;
  external String get typeName;
  external String get errorMessage;
}
