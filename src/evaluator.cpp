#include "pcalc_constexpr/evaluator.h"

#include "clang/AST/APValue.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
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

bool hasForbiddenSourceText(const std::string &expression) {
  return expression.find_first_of(";{}#\n\r") != std::string::npos;
}

class SubsetVisitor : public clang::RecursiveASTVisitor<SubsetVisitor> {
public:
  bool VisitStmt(clang::Stmt *statement) {
    if (llvm::isa<clang::IntegerLiteral, clang::FloatingLiteral,
                  clang::CharacterLiteral, clang::CXXBoolLiteralExpr,
                  clang::ParenExpr, clang::UnaryOperator, clang::BinaryOperator,
                  clang::ConditionalOperator, clang::ImplicitCastExpr,
                  clang::CStyleCastExpr, clang::CXXStaticCastExpr>(statement))
      return true;

    error_ = std::string("unsupported expression node: ") +
             statement->getStmtClassName();
    return false;
  }

  const std::string &error() const { return error_; }

private:
  std::string error_;
};

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
    language_options_.EnableNewConstInterp = true;
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

  const std::string input(expression, expression_length);
  if (hasForbiddenSourceText(input)) {
    fail(*result,
         "declarations, directives, and statement syntax are not allowed");
    return result->status;
  }

  const LanguageConfiguration *configuration = languageConfiguration(language);
  if (!configuration) {
    fail(*result, "unknown expression language standard");
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
  const std::string declaration =
      is_cxx ? "constexpr auto __pcalc_result = (" + input + ");"
             : "static const __typeof__((" + input + ")) __pcalc_result = (" +
                   input + ");";
  const std::string source = aliases + declaration;

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
  SubsetVisitor visitor;
  if (!visitor.TraverseStmt(initializer)) {
    fail(*result, visitor.error());
    return result->status;
  }

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
