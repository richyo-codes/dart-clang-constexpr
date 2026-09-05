#include "pcalc_constexpr/evaluator.h"

#include <cstring>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: pcalc-constexpr [--c|--cxx] '<expression>'\n";
    return 2;
  }

  int32_t language = PCALC_CONSTEXPR_CXX;
  const char *expression = argv[1];
  if (argc == 3) {
    if (std::strcmp(argv[1], "--c") == 0)
      language = PCALC_CONSTEXPR_C;
    else if (std::strcmp(argv[1], "--cxx") != 0) {
      std::cerr << "unknown language option: " << argv[1] << '\n';
      return 2;
    }
    expression = argv[2];
  }

  pcalc_constexpr_result result{};
  pcalc_constexpr_evaluate_language(
      language, expression, std::strlen(expression), &result);
  if (result.status != 0) {
    std::cerr << result.error_message << '\n';
    return 1;
  }

  std::cout << "type=" << result.type_name << " kind=" << result.kind
            << " width=" << result.bit_width
            << " signed=" << result.is_signed << " value=";
  if (result.kind == PCALC_CONSTEXPR_FLOATING)
    std::cout << result.floating_value;
  else
    std::cout << result.integer_value;
  std::cout << '\n';
  return 0;
}
