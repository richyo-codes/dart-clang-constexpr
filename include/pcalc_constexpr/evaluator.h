#ifndef PCALC_CONSTEXPR_EVALUATOR_H
#define PCALC_CONSTEXPR_EVALUATOR_H

#include <stdint.h>

#if defined(_WIN32)
#if defined(PCALC_CONSTEXPR_BUILDING_LIBRARY)
#define PCALC_CONSTEXPR_API __declspec(dllexport)
#else
#define PCALC_CONSTEXPR_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define PCALC_CONSTEXPR_API __attribute__((visibility("default")))
#else
#define PCALC_CONSTEXPR_API
#endif

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
  // Backward-compatible defaults.
  PCALC_CONSTEXPR_C = 0,
  PCALC_CONSTEXPR_CXX = 1,
  PCALC_CONSTEXPR_C99 = 2,
  PCALC_CONSTEXPR_C11 = 3,
  PCALC_CONSTEXPR_C17 = 4,
  PCALC_CONSTEXPR_C23 = 5,
  PCALC_CONSTEXPR_CXX11 = 6,
  PCALC_CONSTEXPR_CXX14 = 7,
  PCALC_CONSTEXPR_CXX17 = 8,
  PCALC_CONSTEXPR_CXX20 = 9,
  PCALC_CONSTEXPR_CXX23 = 10,
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

PCALC_CONSTEXPR_API int32_t
pcalc_constexpr_evaluate(const char *expression, uint32_t expression_length,
                         pcalc_constexpr_result *result);

PCALC_CONSTEXPR_API int32_t pcalc_constexpr_evaluate_language(
    int32_t language, const char *expression, uint32_t expression_length,
    pcalc_constexpr_result *result);

PCALC_CONSTEXPR_API uint32_t pcalc_constexpr_result_size(void);

#ifdef __cplusplus
}
#endif

#endif
