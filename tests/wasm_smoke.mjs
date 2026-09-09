import { performance } from 'node:perf_hooks';
import { initializePcalcClangConstexpr } from '../lib/pcalc_clang_constexpr_loader.js';

const started = performance.now();
const module = await initializePcalcClangConstexpr();
const initialized = performance.now();

function evaluate(language, expression) {
  const before = performance.now();
  const result = module.evaluate(language, expression);
  const after = performance.now();
  return {expression, ...result, elapsedMs: after - before};
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
