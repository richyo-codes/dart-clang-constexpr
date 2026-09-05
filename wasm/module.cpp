#include "pcalc_constexpr/evaluator.h"

// This translation unit gives Emscripten an executable link target. The public
// evaluator functions are retained through EXPORTED_FUNCTIONS.
static_assert(sizeof(pcalc_constexpr_result) > 0);
