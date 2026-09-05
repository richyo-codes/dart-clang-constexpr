#ifndef PCALC_CONSTEXPR_EVALUATOR_H
#define PCALC_CONSTEXPR_EVALUATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum pcalc_constexpr_kind {
  PCALC_CONSTEXPR_ERROR = 0,
  PCALC_CONSTEXPR_INTEGER = 1,
  PCALC_CONSTEXPR_FLOATING = 2,
  PCALC_CONSTEXPR_BOOLEAN = 3,
  PCALC_CONSTEXPR_CHARACTER = 4,
};

enum pcalc_constexpr_language {
  PCALC_CONSTEXPR_C = 0,
  PCALC_CONSTEXPR_CXX = 1,
};

typedef struct pcalc_constexpr_result {
  int32_t status;
  int32_t kind;
  int32_t bit_width;
  int32_t is_signed;
  double floating_value;
  char integer_value[160];
  char type_name[96];
  char error_message[1024];
} pcalc_constexpr_result;

int32_t pcalc_constexpr_evaluate(const char *expression,
                                 uint32_t expression_length,
                                 pcalc_constexpr_result *result);

int32_t pcalc_constexpr_evaluate_language(
    int32_t language, const char *expression, uint32_t expression_length,
    pcalc_constexpr_result *result);

uint32_t pcalc_constexpr_result_size(void);

#ifdef __cplusplus
}
#endif

#endif
