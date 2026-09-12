#include "pcalc_constexpr/evaluator.h"

// Keep the test oracle active in Release and MinSizeRel builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <string>

pcalc_constexpr_result evaluate(const char *expression) {
  pcalc_constexpr_result result{};
  pcalc_constexpr_evaluate(expression, std::strlen(expression), &result);
  return result;
}

pcalc_constexpr_result evaluateC(const char *expression) {
  pcalc_constexpr_result result{};
  pcalc_constexpr_evaluate_language(
      PCALC_CONSTEXPR_C, expression, std::strlen(expression), &result);
  return result;
}

int main() {
  auto arithmetic = evaluate("1 + 2 * 3");
  assert(arithmetic.status == 0);
  assert(std::string(arithmetic.integer_value) == "7");

  auto character = evaluate("(char)255");
  assert(character.status == 0);
  assert(character.kind == PCALC_CONSTEXPR_CHARACTER);
  assert(character.bit_width == 8);
  assert(std::string(character.integer_value) == "-1");

  auto byte = evaluate("(uint8_t)270");
  assert(byte.status == 0);
  assert(byte.bit_width == 8);
  assert(std::string(byte.integer_value) == "14");

  auto wide = evaluate("1ULL << 63");
  assert(wide.status == 0);
  assert(std::string(wide.integer_value) == "9223372036854775808");

  // A closure alone is not a numeric calculator result.
  for (const char *lambda : {
           "[]{}",
           "[value = 4]{}",
           "[](auto value){}",
       }) {
    auto rejected = evaluate(lambda);
    assert(rejected.status != 0);
  }

  auto invoked_lambda = evaluate("[](int value) { return value + 1; }(41)");
  assert(invoked_lambda.status == 0);
  assert(std::string(invoked_lambda.integer_value) == "42");

  for (const char *input : {
           "[] { int n = 1; for (int i = 2; i <= 5; ++i) n *= i; return n; }()",
           "[] { auto f = [](auto self, int n) -> int { return n < 2 ? 1 : n * self(self, n-1); }; return f(f, 5); }()",
           "[]() consteval { return 120; }()",
           "[n = 119] { return n + 1; }()",
           "[] { int values[] = {100, 20}; return values[0] + values[1]; }()",
           "[] { struct V { int n; constexpr int get() const { return n; } }; return V{120}.get(); }()",
           "\n100 +\n20 // trailing comment",
           "120 /* ; { } # */",
           "'{' - '{' + 120",
       }) {
    auto value = evaluate(input);
    assert(value.status == 0);
    assert(std::string(value.integer_value) == "120");
  }

  for (const char *input : {
           "1; int injected = 2",
           "1); constexpr int injected = (2",
           "1) /* escape wrapper */",
           "\n#define X 1\nX",
           "\n%:define X 1\nX",
           "\n#include <cmath>\n1",
           "[] { while (true) {} return 1; }()",
           "[] { static int n = 0; return ++n; }()",
           "[] { int n = 0; return 1 / n; }()",
           "[] { return 1; }", // nonnumeric closure
           "\"text\"", // pointer result
       }) {
    assert(evaluate(input).status != 0);
  }

  const char nul_input[] = {'1', '\0', '+', '2'};
  pcalc_constexpr_result invalid{};
  pcalc_constexpr_evaluate(nul_input, sizeof(nul_input), &invalid);
  assert(invalid.status != 0);
  assert(evaluate(std::string(65537, '1').c_str()).status != 0);

  auto c_multiline = evaluateC("1 +\n2");
  assert(c_multiline.status == 0);
  assert(std::string(c_multiline.integer_value) == "3");
  assert(evaluateC("[] { return 1; }()").status != 0);

  pcalc_constexpr_result cxx14{};
  const char *call = "[] { return 1; }()";
  pcalc_constexpr_evaluate_language(PCALC_CONSTEXPR_CXX14, call,
                                  std::strlen(call), &cxx14);
  assert(cxx14.status != 0);

  auto c_cast = evaluateC("(unsigned char)270");
  assert(c_cast.status == 0);
  assert(c_cast.bit_width == 8);
  assert(std::string(c_cast.integer_value) == "14");

  auto c_shift = evaluateC("0xffu << 8");
  assert(c_shift.status == 0);
  assert(std::string(c_shift.integer_value) == "65280");

  auto cxx_cast = evaluate("static_cast<unsigned char>(270)");
  assert(cxx_cast.status == 0);
  assert(std::string(cxx_cast.integer_value) == "14");

  auto c_rejects_cxx_cast = evaluateC("static_cast<unsigned char>(270)");
  assert(c_rejects_cxx_cast.status != 0);

  return 0;
}
