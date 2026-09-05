#include "pcalc_constexpr/evaluator.h"

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

  auto rejected = evaluate("[] { return 1; }()");
  assert(rejected.status != 0);

  auto c_cast = evaluateC("(unsigned char)270");
  assert(c_cast.status == 0);
  assert(c_cast.bit_width == 8);
  assert(std::string(c_cast.integer_value) == "14");

  auto c_shift = evaluateC("0xffu << 8");
  assert(c_shift.status == 0);
  assert(std::string(c_shift.integer_value) == "65280");

  return 0;
}
