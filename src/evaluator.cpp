#include "pcalc_constexpr/evaluator.h"

#include "clang/AST/APValue.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/ModuleLoader.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/TargetParser/Triple.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char *kResultName = "__pcalc_result";

constexpr const char *kCxxCalculatorPrelude = R"cpp(
namespace std {
constexpr long double pi = 3.141592653589793238462643383279502884L;
namespace numbers {
constexpr long double pi = std::pi;
}

template <class T, T Minimum, T Maximum> struct integer_limits {
  static constexpr T min() { return Minimum; }
  static constexpr T lowest() { return Minimum; }
  static constexpr T max() { return Maximum; }
};
template <class T> struct numeric_limits;
template <> struct numeric_limits<signed char>
    : integer_limits<signed char, -128, 127> {};
template <> struct numeric_limits<unsigned char>
    : integer_limits<unsigned char, 0, 255> {};
template <> struct numeric_limits<short>
    : integer_limits<short, -32768, 32767> {};
template <> struct numeric_limits<unsigned short>
    : integer_limits<unsigned short, 0, 65535> {};
template <> struct numeric_limits<int>
    : integer_limits<int, (-2147483647 - 1), 2147483647> {};
template <> struct numeric_limits<unsigned int>
    : integer_limits<unsigned int, 0U, 4294967295U> {};
template <> struct numeric_limits<long>
    : integer_limits<long, (-2147483647L - 1), 2147483647L> {};
template <> struct numeric_limits<unsigned long>
    : integer_limits<unsigned long, 0UL, 4294967295UL> {};
template <> struct numeric_limits<long long>
    : integer_limits<long long, (-9223372036854775807LL - 1),
                     9223372036854775807LL> {};
template <> struct numeric_limits<unsigned long long>
    : integer_limits<unsigned long long, 0ULL, 18446744073709551615ULL> {};

constexpr int abs(int value) { return value < 0 ? -value : value; }
constexpr long abs(long value) { return value < 0 ? -value : value; }
constexpr long long abs(long long value) { return value < 0 ? -value : value; }
constexpr float abs(float value) { return value < 0 ? -value : value; }
constexpr double abs(double value) { return value < 0 ? -value : value; }
constexpr long double abs(long double value) { return value < 0 ? -value : value; }

template <class T> constexpr T trunc_impl(T value) {
  return static_cast<T>(static_cast<long long>(value));
}
template <class T> constexpr T ceil_impl(T value) {
  return trunc_impl(value) < value ? trunc_impl(value) + 1 : trunc_impl(value);
}
template <class T> constexpr T floor_impl(T value) {
  return trunc_impl(value) > value ? trunc_impl(value) - 1 : trunc_impl(value);
}
template <class T> constexpr T round_impl(T value) {
  return value < 0 ? ceil_impl(value - T(0.5)) : floor_impl(value + T(0.5));
}
template <class T> constexpr T sqrt_impl(T value, T current, T previous) {
  return current == previous ? current
                             : sqrt_impl(value, (current + value / current) / 2,
                                         current);
}
template <class T> constexpr T pow_unsigned(T base, unsigned long long exponent) {
  return exponent == 0
             ? T(1)
             : exponent % 2 == 0
                   ? pow_unsigned(base * base, exponent / 2)
                   : base * pow_unsigned(base * base, exponent / 2);
}
template <class T> constexpr T pow_integral(T base, long long exponent) {
  return exponent < 0 ? T(1) / pow_unsigned(base, -exponent)
                      : pow_unsigned(base, exponent);
}
template <class T> constexpr T reduce_angle(T value) {
  return value - T(2) * T(pi) * floor_impl((value + T(pi)) / (T(2) * T(pi)));
}
template <class T>
constexpr T sin_series(T value, int iteration, T term, T sum) {
  return iteration == 14
             ? sum
             : sin_series(value, iteration + 1,
                          -term * value * value /
                              T((2 * iteration) * (2 * iteration + 1)),
                          sum - term * value * value /
                                    T((2 * iteration) * (2 * iteration + 1)));
}
template <class T>
constexpr T cos_series(T value, int iteration, T term, T sum) {
  return iteration == 14
             ? sum
             : cos_series(value, iteration + 1,
                          -term * value * value /
                              T((2 * iteration - 1) * (2 * iteration)),
                          sum - term * value * value /
                                    T((2 * iteration - 1) * (2 * iteration)));
}

constexpr float ceil(float value) { return ceil_impl(value); }
constexpr double ceil(double value) { return ceil_impl(value); }
constexpr long double ceil(long double value) { return ceil_impl(value); }
constexpr float floor(float value) { return floor_impl(value); }
constexpr double floor(double value) { return floor_impl(value); }
constexpr long double floor(long double value) { return floor_impl(value); }
constexpr float trunc(float value) { return trunc_impl(value); }
constexpr double trunc(double value) { return trunc_impl(value); }
constexpr long double trunc(long double value) { return trunc_impl(value); }
constexpr float round(float value) { return round_impl(value); }
constexpr double round(double value) { return round_impl(value); }
constexpr long double round(long double value) { return round_impl(value); }
constexpr float sqrt(float value) {
  return value < 0 ? 0.0F / 0.0F
                   : value == 0 ? 0 : sqrt_impl(value, value, 0.0F);
}
constexpr double sqrt(double value) {
  return value < 0 ? 0.0 / 0.0 : value == 0 ? 0 : sqrt_impl(value, value, 0.0);
}
constexpr long double sqrt(long double value) {
  return value < 0 ? 0.0L / 0.0L : value == 0 ? 0 : sqrt_impl(value, value, 0.0L);
}
constexpr double pow(double base, int exponent) {
  return pow_integral(base, exponent);
}
constexpr double pow(int base, int exponent) {
  return pow_integral(static_cast<double>(base), exponent);
}
constexpr double pow(double base, double exponent) {
  return exponent == trunc_impl(exponent)
             ? pow_integral(base, static_cast<long long>(exponent))
             : 0.0 / 0.0;
}
constexpr float pow(float base, int exponent) {
  return pow_integral(base, exponent);
}
constexpr long double pow(long double base, int exponent) {
  return pow_integral(base, exponent);
}
constexpr double sin(double value) {
  return sin_series(reduce_angle(value), 1, reduce_angle(value),
                    reduce_angle(value));
}
constexpr double cos(double value) {
  return cos_series(reduce_angle(value), 1, 1.0, 1.0);
}
constexpr double tan(double value) { return sin(value) / cos(value); }
constexpr float fmin(float lhs, float rhs) { return rhs < lhs ? rhs : lhs; }
constexpr double fmin(double lhs, double rhs) { return rhs < lhs ? rhs : lhs; }
constexpr long double fmin(long double lhs, long double rhs) { return rhs < lhs ? rhs : lhs; }
constexpr float fmax(float lhs, float rhs) { return lhs < rhs ? rhs : lhs; }
constexpr double fmax(double lhs, double rhs) { return lhs < rhs ? rhs : lhs; }
constexpr long double fmax(long double lhs, long double rhs) { return lhs < rhs ? rhs : lhs; }

template <class T> constexpr T min(const T &lhs, const T &rhs) {
  return rhs < lhs ? rhs : lhs;
}
template <class T> constexpr T max(const T &lhs, const T &rhs) {
  return lhs < rhs ? rhs : lhs;
}
template <class T>
constexpr T clamp(const T &value, const T &low, const T &high) {
  return value < low ? low : high < value ? high : value;
}
} // namespace std

using std::abs;
using std::ceil;
using std::floor;
using std::trunc;
using std::round;
using std::sqrt;
using std::pow;
using std::sin;
using std::cos;
using std::tan;
using std::fmin;
using std::fmax;
constexpr long double pi = std::pi;
#define CHAR_BIT 8
#define INT8_MIN (-127 - 1)
#define INT8_MAX 127
#define UINT8_MAX 255U
#define INT16_MIN (-32767 - 1)
#define INT16_MAX 32767
#define UINT16_MAX 65535U
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295U
#define INT64_MIN (-9223372036854775807LL - 1)
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 18446744073709551615ULL
#define INT_MIN INT32_MIN
#define INT_MAX INT32_MAX
#define UINT_MAX UINT32_MAX
#define LLONG_MIN INT64_MIN
#define LLONG_MAX INT64_MAX
#define ULLONG_MAX UINT64_MAX
)cpp";

struct LanguageConfiguration {
  bool is_cxx;
  clang::Language language;
  clang::LangStandard::Kind standard;
};

const LanguageConfiguration *languageConfiguration(int32_t language) {
  static constexpr LanguageConfiguration kC99{
      false, clang::Language::C, clang::LangStandard::lang_c99};
  static constexpr LanguageConfiguration kC11{
      false, clang::Language::C, clang::LangStandard::lang_c11};
  static constexpr LanguageConfiguration kC17{
      false, clang::Language::C, clang::LangStandard::lang_c17};
  static constexpr LanguageConfiguration kC23{
      false, clang::Language::C, clang::LangStandard::lang_c23};
  static constexpr LanguageConfiguration kCxx11{
      true, clang::Language::CXX, clang::LangStandard::lang_cxx11};
  static constexpr LanguageConfiguration kCxx14{
      true, clang::Language::CXX, clang::LangStandard::lang_cxx14};
  static constexpr LanguageConfiguration kCxx17{
      true, clang::Language::CXX, clang::LangStandard::lang_cxx17};
  static constexpr LanguageConfiguration kCxx20{
      true, clang::Language::CXX, clang::LangStandard::lang_cxx20};
  static constexpr LanguageConfiguration kCxx23{
      true, clang::Language::CXX, clang::LangStandard::lang_cxx23};

  switch (language) {
  case PCALC_CONSTEXPR_C:
  case PCALC_CONSTEXPR_C23:
    return &kC23;
  case PCALC_CONSTEXPR_CXX:
  case PCALC_CONSTEXPR_CXX20:
    return &kCxx20;
  case PCALC_CONSTEXPR_C99:
    return &kC99;
  case PCALC_CONSTEXPR_C11:
    return &kC11;
  case PCALC_CONSTEXPR_C17:
    return &kC17;
  case PCALC_CONSTEXPR_CXX11:
    return &kCxx11;
  case PCALC_CONSTEXPR_CXX14:
    return &kCxx14;
  case PCALC_CONSTEXPR_CXX17:
    return &kCxx17;
  case PCALC_CONSTEXPR_CXX23:
    return &kCxx23;
  default:
    return nullptr;
  }
}

void copyText(char *destination, size_t capacity, const std::string &value) {
  if (capacity == 0)
    return;
  const size_t count = std::min(capacity - 1, value.size());
  std::memcpy(destination, value.data(), count);
  destination[count] = '\0';
}

void fail(pcalc_constexpr_result &result, const std::string &message) {
  result.status = 1;
  result.kind = PCALC_CONSTEXPR_ERROR;
  copyText(result.error_message, sizeof(result.error_message), message);
}

// Validate the wrapper boundary using tokens, not characters: punctuation in
// literals/comments is harmless, and lambda bodies may contain statements.
bool isSingleExpression(const std::string &input,
                        const LanguageConfiguration &configuration) {
  clang::LangOptions options;
  std::vector<std::string> includes;
  clang::LangOptions::setLangDefaults(
      options, configuration.language, llvm::Triple("wasm32-unknown-unknown"),
      includes, configuration.standard);
  clang::Lexer lexer(clang::SourceLocation(), options, input.data(),
                     input.data(), input.data() + input.size());
  std::vector<clang::tok::TokenKind> delimiters;
  clang::Token token;
  do {
    lexer.LexFromRawLexer(token);
    const auto kind = token.getKind();
    if (kind == clang::tok::hash || kind == clang::tok::hashhash ||
        kind == clang::tok::unknown)
      return false;
    if (kind == clang::tok::semi && delimiters.empty())
      return false;
    if (kind == clang::tok::l_paren || kind == clang::tok::l_square ||
        kind == clang::tok::l_brace) {
      delimiters.push_back(kind);
    } else if (kind == clang::tok::r_paren || kind == clang::tok::r_square ||
               kind == clang::tok::r_brace) {
      const auto opening = kind == clang::tok::r_paren ? clang::tok::l_paren
                           : kind == clang::tok::r_square ? clang::tok::l_square
                                                        : clang::tok::l_brace;
      if (delimiters.empty() || delimiters.back() != opening)
        return false;
      delimiters.pop_back();
    }
  } while (!token.is(clang::tok::eof));
  return delimiters.empty();
}

clang::VarDecl *findResult(clang::ASTContext &context) {
  for (clang::Decl *decl : context.getTranslationUnitDecl()->decls()) {
    auto *variable = llvm::dyn_cast<clang::VarDecl>(decl);
    if (variable && variable->getName() == kResultName)
      return variable;
  }
  return nullptr;
}

class DiagnosticCollector : public clang::DiagnosticConsumer {
public:
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &diagnostic) override {
    clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);
    if (level < clang::DiagnosticsEngine::Error)
      return;
    llvm::SmallString<256> message;
    diagnostic.FormatDiagnostic(message);
    if (!text_.empty())
      text_ += " | ";
    text_ += std::string(message);
  }

  std::string text() const {
    return text_.empty() ? "expression could not be parsed" : text_;
  }

private:
  std::string text_;
};

class ParsedTranslationUnit {
public:
  ParsedTranslationUnit(const std::string &source,
                        const LanguageConfiguration &configuration)
      : filesystem_(llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>()),
        file_manager_(clang::FileSystemOptions(), filesystem_),
        diagnostics_(clang::DiagnosticIDs::create(), diagnostic_options_,
                     &diagnostic_collector_, false),
        source_manager_(diagnostics_, file_manager_) {
    target_options_ = std::make_shared<clang::TargetOptions>();
    target_options_->Triple = "wasm32-unknown-unknown";
    target_ =
        clang::TargetInfo::CreateTargetInfo(diagnostics_, *target_options_);
    if (!target_)
      return;

    std::vector<std::string> includes;
    const llvm::Triple triple(target_options_->Triple);
    clang::LangOptions::setLangDefaults(language_options_, configuration.language,
                                        triple, includes, configuration.standard);
    // LLVM 22's experimental interpreter can ignore the loop step limit.
    // Keep native and WASM on the established evaluator until all toolchains
    // include and verify upstream llvm/llvm-project#176150.
    language_options_.EnableNewConstInterp = false;
    language_options_.ConstexprStepLimit = 100000;

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(
        source, configuration.is_cxx ? "pcalc_expression.cc"
                                      : "pcalc_expression.c");
    source_manager_.setMainFileID(
        source_manager_.createFileID(std::move(buffer)));

    header_search_ = std::make_unique<clang::HeaderSearch>(
        header_search_options_, source_manager_, diagnostics_,
        language_options_, target_.get());
    preprocessor_ = std::make_unique<clang::Preprocessor>(
        preprocessor_options_, diagnostics_, language_options_, source_manager_,
        *header_search_, module_loader_);
    preprocessor_->Initialize(*target_);
    preprocessor_->getBuiltinInfo().initializeBuiltins(
        preprocessor_->getIdentifierTable(), language_options_);
    context_ = std::make_unique<clang::ASTContext>(
        language_options_, source_manager_, preprocessor_->getIdentifierTable(),
        preprocessor_->getSelectorTable(), preprocessor_->getBuiltinInfo(),
        clang::TU_Complete);
    context_->InitBuiltinTypes(*target_);
    sema_ = std::make_unique<clang::Sema>(*preprocessor_, *context_, consumer_);
    clang::ParseAST(*sema_, false, false);
    valid_ = diagnostics_.getNumErrors() == 0;
  }

  clang::ASTContext &context() { return *context_; }
  std::string diagnosticsText() const { return diagnostic_collector_.text(); }
  bool valid() const { return valid_; }

private:
  llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> filesystem_;
  clang::FileManager file_manager_;
  clang::DiagnosticOptions diagnostic_options_;
  DiagnosticCollector diagnostic_collector_;
  clang::DiagnosticsEngine diagnostics_;
  clang::SourceManager source_manager_;
  std::shared_ptr<clang::TargetOptions> target_options_;
  llvm::IntrusiveRefCntPtr<clang::TargetInfo> target_;
  clang::LangOptions language_options_;
  clang::HeaderSearchOptions header_search_options_;
  clang::PreprocessorOptions preprocessor_options_;
  clang::TrivialModuleLoader module_loader_;
  clang::ASTConsumer consumer_;
  std::unique_ptr<clang::HeaderSearch> header_search_;
  std::unique_ptr<clang::Preprocessor> preprocessor_;
  std::unique_ptr<clang::ASTContext> context_;
  std::unique_ptr<clang::Sema> sema_;
  bool valid_ = false;
};

} // namespace

extern "C" int32_t pcalc_constexpr_evaluate(const char *expression,
                                            uint32_t expression_length,
                                            pcalc_constexpr_result *result) {
  return pcalc_constexpr_evaluate_language(PCALC_CONSTEXPR_CXX, expression,
                                           expression_length, result);
}

extern "C" uint32_t pcalc_constexpr_result_size(void) {
  return sizeof(pcalc_constexpr_result);
}

extern "C" int32_t
pcalc_constexpr_evaluate_language(int32_t language, const char *expression,
                                  uint32_t expression_length,
                                  pcalc_constexpr_result *result) {
  if (!result)
    return 1;
  std::memset(result, 0, sizeof(*result));
  if (!expression || expression_length == 0) {
    fail(*result, "expression is empty");
    return result->status;
  }

  if (expression_length > 65536 ||
      std::memchr(expression, '\0', expression_length)) {
    fail(*result, "expression must be at most 65536 bytes without embedded NULs");
    return result->status;
  }

  const LanguageConfiguration *configuration = languageConfiguration(language);
  if (!configuration) {
    fail(*result, "unknown expression language standard");
    return result->status;
  }

  const std::string input(expression, expression_length);
  if (!isSingleExpression(input, *configuration)) {
    fail(*result, "expected one expression without preprocessor directives or unmatched delimiters");
    return result->status;
  }

  const bool is_cxx = configuration->is_cxx;
  const std::string aliases =
      is_cxx
          ? "using int8_t = signed char; using uint8_t = unsigned char; "
            "using int16_t = short; using uint16_t = unsigned short; "
            "using int32_t = int; using uint32_t = unsigned int; "
            "using int64_t = long long; using uint64_t = unsigned long long; "
          : "typedef signed char int8_t; typedef unsigned char uint8_t; "
            "typedef short int16_t; typedef unsigned short uint16_t; "
            "typedef int int32_t; typedef unsigned int uint32_t; "
            "typedef long long int64_t; typedef unsigned long long uint64_t; ";
  const std::string prelude = is_cxx ? kCxxCalculatorPrelude : "";
  const std::string declaration =
      is_cxx ? "constexpr auto __pcalc_result = (\n" + input + "\n);"
             : "static const __typeof__((\n" + input + "\n)) __pcalc_result = (\n" +
                   input + "\n);";
  const std::string source = aliases + prelude + declaration;

  ParsedTranslationUnit translation_unit(source, *configuration);
  if (!translation_unit.valid()) {
    fail(*result, translation_unit.diagnosticsText());
    return result->status;
  }

  clang::ASTContext &context = translation_unit.context();
  clang::VarDecl *variable = findResult(context);
  if (!variable || !variable->hasInit()) {
    fail(*result, "internal error: result expression was not found");
    return result->status;
  }

  clang::Expr *initializer = variable->getInit()->IgnoreImplicit();
  clang::Expr::EvalResult evaluated;
  if (!initializer->EvaluateAsConstantExpr(evaluated, context)) {
    fail(*result, "expression is not a supported constant expression");
    return result->status;
  }

  const clang::QualType type = variable->getType();
  copyText(result->type_name, sizeof(result->type_name), type.getAsString());
  result->bit_width = static_cast<int32_t>(context.getTypeSize(type));

  if (type->isBooleanType()) {
    result->kind = PCALC_CONSTEXPR_BOOLEAN;
    result->is_signed = 0;
  } else if (type->isAnyCharacterType()) {
    result->kind = PCALC_CONSTEXPR_CHARACTER;
    result->is_signed = type->isSignedIntegerType();
  } else if (type->isIntegralOrEnumerationType()) {
    result->kind = PCALC_CONSTEXPR_INTEGER;
    result->is_signed = type->isSignedIntegerOrEnumerationType();
  } else if (type->isRealFloatingType()) {
    result->kind = PCALC_CONSTEXPR_FLOATING;
    result->floating_value = evaluated.Val.getFloat().convertToDouble();
    result->status = 0;
    return 0;
  } else {
    fail(*result, "constant result type is not supported");
    return result->status;
  }

  if (!evaluated.Val.isInt()) {
    fail(*result, "integer result did not produce an integer APValue");
    return result->status;
  }
  llvm::SmallString<160> integer_text;
  evaluated.Val.getInt().toString(integer_text, 10);
  copyText(result->integer_value, sizeof(result->integer_value),
           std::string(integer_text));
  result->status = 0;
  return 0;
}
