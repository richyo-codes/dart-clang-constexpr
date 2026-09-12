import createModule from './dart_clang_constexpr.js';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

function readCString(heap, offset, capacity) {
  const bytes = heap.subarray(offset, offset + capacity);
  const end = bytes.indexOf(0);
  return decoder.decode(end < 0 ? bytes : bytes.subarray(0, end));
}

export async function initializeDartClangConstexpr(options = {}) {
  const wasmUrl = options.wasmUrl ?? new URL(
    './dart_clang_constexpr.wasm',
    import.meta.url,
  ).href;
  const module = await createModule({
    locateFile(path) {
      return path.endsWith('.wasm') ? wasmUrl : path;
    },
  });

  return {
    evaluate(language, expression) {
      const encoded = encoder.encode(expression);
      const expressionPointer = module._malloc(encoded.length || 1);
      const resultPointer = module._malloc(
        module._pcalc_constexpr_result_size(),
      );
      try {
        module.HEAPU8.set(encoded, expressionPointer);
        module._pcalc_constexpr_evaluate_language(
          language,
          expressionPointer,
          encoded.length,
          resultPointer,
        );
        const view = new DataView(module.HEAPU8.buffer);
        const status = view.getInt32(resultPointer, true);
        const kind = view.getInt32(resultPointer + 4, true);
        const floatingValue = view.getFloat64(resultPointer + 16, true);
        return {
          status,
          kind,
          bitWidth: view.getInt32(resultPointer + 8, true),
          isSigned: view.getInt32(resultPointer + 12, true) !== 0,
          floatingValue,
          displayText: kind === 2
            ? String(floatingValue)
            : readCString(module.HEAPU8, resultPointer + 24, 160),
          typeName: readCString(module.HEAPU8, resultPointer + 184, 96),
          errorMessage: readCString(module.HEAPU8, resultPointer + 280, 1024),
        };
      } finally {
        module._free(resultPointer);
        module._free(expressionPointer);
      }
    },
  };
}
