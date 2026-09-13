import { performance } from 'node:perf_hooks';
import assert from 'node:assert/strict';
import { initializeDartClangConstexpr } from '../lib/dart_clang_constexpr_loader.js';

const started = performance.now();
const module = await initializeDartClangConstexpr();
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

const expected = [
  ['7', 32, true],
  ['-1', 8, true],
  ['9223372036854775808', 64, false],
  ['14', 8, false],
];
results.forEach((result, index) => {
  assert.equal(result.status, 0, result.errorMessage);
  assert.deepEqual(
    [result.displayText, result.bitWidth, result.isSigned],
    expected[index],
    result.expression,
  );
});
for (let language = 0; language <= 10; language++) {
  const result = evaluate(language, '40 + 2');
  assert.equal(result.status, 0, result.errorMessage);
  assert.equal(result.displayText, '42', `language ${language}`);
}
assert.notEqual(evaluate(1, '1 +').status, 0);

for (const [expression, expected] of [
  ['abs(-42)', '42'],
  ['ceil(4.2)', '5'],
  ['std::floor(4.8)', '4'],
  ['round(4.5)', '5'],
  ['sqrt(81.0)', '9'],
  ['pow(2, 10)', '1024'],
  ['std::pow(2.0, -3)', '0.125'],
  ['sin(pi / 2)', '1'],
  ['cos(0.0)', '1'],
  ['tan(std::numbers::pi / 4)', '1'],
  ['std::min(9, 3)', '3'],
  ['std::max(9, 3)', '9'],
  ['std::clamp(99, 0, 10)', '10'],
  ['INT_MAX', '2147483647'],
  ['UINT64_MAX', '18446744073709551615'],
  ['std::numeric_limits<int8_t>::max()', '127'],
  ['sizeof(uint64_t)', '8'],
]) {
  const result = evaluate(1, expression);
  assert.equal(result.status, 0, result.errorMessage);
  assert.ok(
    Math.abs(Number(result.displayText) - Number(expected)) < 1e-12,
    expression,
  );
}

for (const [expression, expectedValue] of [
  ['abs(-42)', '42'],
  ['std::abs(-42.5)', '42.5'],
  ['ceil(4.2)', '5'],
  ['std::floor(4.8)', '4'],
  ['round(4.5)', '5'],
  ['trunc(-4.8)', '-4'],
  ['sqrt(81.0)', '9'],
  ['fmin(7.5, 2.5)', '2.5'],
  ['std::fmax(7.5, 2.5)', '7.5'],
  ['std::min(9, 3)', '3'],
  ['std::max(9, 3)', '9'],
  ['std::clamp(99, 0, 10)', '10'],
]) {
  const result = evaluate(1, expression);
  assert.equal(result.status, 0, result.errorMessage);
  assert.equal(result.displayText, expectedValue, expression);
}

for (const expression of [
  '[] { int n = 1; for (int i = 2; i <= 5; ++i) n *= i; return n; }()',
  '[] { auto f = [](auto self, int n) -> int { return n < 2 ? 1 : n * self(self, n-1); }; return f(f, 5); }()',
  '[]() consteval { return 120; }()',
  '[n = 119] { return n + 1; }()',
  '\n100 +\n20 // comment',
  "'{' - '{' + 120",
]) {
  const result = evaluate(1, expression);
  assert.equal(result.status, 0, result.errorMessage);
  assert.equal(result.displayText, '120', expression);
}
for (const expression of [
  '1); constexpr int injected = (2',
  '\n#define X 1\nX',
  '\n%:define X 1\nX',
  '[] { while (true) {} return 1; }()',
  '[] { static int n = 0; return ++n; }()',
  '[] { return 1; }',
]) {
  assert.notEqual(evaluate(1, expression).status, 0, expression);
}
assert.notEqual(evaluate(0, '[] { return 1; }()').status, 0);
console.log('Expanded constant-expression and limit checks passed.');
