import { performance } from 'node:perf_hooks';
import createModule from '../build-wasm/pcalc-constexpr-wasm.js';

const started = performance.now();
const module = await createModule();
const initialized = performance.now();

function readCString(offset, capacity) {
  const bytes = module.HEAPU8.subarray(offset, offset + capacity);
  const end = bytes.indexOf(0);
  return new TextDecoder().decode(end < 0 ? bytes : bytes.subarray(0, end));
}

function evaluate(language, expression) {
  const encoded = new TextEncoder().encode(expression);
  const expressionPointer = module._malloc(encoded.length);
  const resultPointer = module._malloc(module._pcalc_constexpr_result_size());
  module.HEAPU8.set(encoded, expressionPointer);

  const before = performance.now();
  module._pcalc_constexpr_evaluate_language(
    language,
    expressionPointer,
    encoded.length,
    resultPointer,
  );
  const after = performance.now();
  const view = new DataView(module.HEAPU8.buffer);
  const result = {
    expression,
    status: view.getInt32(resultPointer, true),
    kind: view.getInt32(resultPointer + 4, true),
    bitWidth: view.getInt32(resultPointer + 8, true),
    isSigned: view.getInt32(resultPointer + 12, true),
    floatingValue: view.getFloat64(resultPointer + 16, true),
    integerValue: readCString(resultPointer + 24, 160),
    typeName: readCString(resultPointer + 184, 96),
    errorMessage: readCString(resultPointer + 280, 1024),
    elapsedMs: after - before,
  };

  module._free(resultPointer);
  module._free(expressionPointer);
  return result;
}

const results = [
  evaluate(1, '1 + 2 * 3'),
  evaluate(1, '(char)255'),
  evaluate(1, '1ULL << 63'),
  evaluate(0, '(unsigned char)270'),
];

console.log(JSON.stringify({
  initializationMs: initialized - started,
  results,
}, null, 2));

if (results.some((result) => result.status !== 0))
  process.exitCode = 1;
