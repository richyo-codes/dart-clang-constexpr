#include "pcalc_constexpr/evaluator.h"

#include "clang/AST/APValue.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/VirtualFileSystem.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char *kResultName = "__pcalc_result";

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

bool parseTranslationUnit(const std::string &source, bool is_cxx,
                          clang::TextDiagnosticBuffer &diagnostics,
                          clang::CompilerInstance &compiler) {
  std::vector<std::string> argument_storage = {
      "-x",
      is_cxx ? "c++" : "c",
      is_cxx ? "-std=c++20" : "-std=c23",
      "-fsyntax-only",
      "-fexperimental-new-constant-interpreter",
      "-fconstexpr-steps=100000",
      "-triple",
      "wasm32-unknown-unknown"};
  std::vector<const char *> arguments;
  arguments.reserve(argument_storage.size());
  for (const std::string &argument : argument_storage)
    arguments.push_back(argument.c_str());

  auto filesystem = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
  auto diagnostic_options = std::make_shared<clang::DiagnosticOptions>();
  auto diagnostic_engine = clang::CompilerInstance::createDiagnostics(
      *filesystem, *diagnostic_options, &diagnostics, false);
  if (!clang::CompilerInvocation::CreateFromArgs(
          compiler.getInvocation(), arguments, *diagnostic_engine,
          "pcalc-constexpr"))
    return false;

  compiler.setVirtualFileSystem(filesystem);
  compiler.setDiagnostics(diagnostic_engine);
  if (!compiler.createTarget())
    return false;
  compiler.createFileManager();
  compiler.createSourceManager();
  auto buffer = llvm::MemoryBuffer::getMemBufferCopy(
      source, is_cxx ? "pcalc_expression.cc" : "pcalc_expression.c");
  compiler.getSourceManager().setMainFileID(
      compiler.getSourceManager().createFileID(std::move(buffer)));
  compiler.createPreprocessor(clang::TU_Complete);
  compiler.createASTContext();
  clang::ASTConsumer consumer;
  clang::ParseAST(compiler.getPreprocessor(), &consumer,
                  compiler.getASTContext(), false, clang::TU_Complete);
  return diagnostics.getNumErrors() == 0;
}

std::string diagnosticsText(clang::TextDiagnosticBuffer &diagnostics) {
  std::string result;
  for (auto entry = diagnostics.err_begin(); entry != diagnostics.err_end();
       ++entry) {
    if (!result.empty())
      result += " | ";
    result += entry->second;
  }
  return result.empty() ? "expression could not be parsed" : result;
}

} // namespace

extern "C" int32_t pcalc_constexpr_evaluate(
    const char *expression, uint32_t expression_length,
    pcalc_constexpr_result *result) {
  return pcalc_constexpr_evaluate_language(
      PCALC_CONSTEXPR_CXX, expression, expression_length, result);
}

extern "C" uint32_t pcalc_constexpr_result_size(void) {
  return sizeof(pcalc_constexpr_result);
}

extern "C" int32_t pcalc_constexpr_evaluate_language(
    int32_t language, const char *expression, uint32_t expression_length,
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
    fail(*result, "declarations, directives, and statement syntax are not allowed");
    return result->status;
  }

  if (language != PCALC_CONSTEXPR_C && language != PCALC_CONSTEXPR_CXX) {
    fail(*result, "unknown expression language");
    return result->status;
  }

  const bool is_cxx = language == PCALC_CONSTEXPR_CXX;
  const std::string aliases = is_cxx
      ? "using int8_t = signed char; using uint8_t = unsigned char; "
        "using int16_t = short; using uint16_t = unsigned short; "
        "using int32_t = int; using uint32_t = unsigned int; "
        "using int64_t = long long; using uint64_t = unsigned long long; "
      : "typedef signed char int8_t; typedef unsigned char uint8_t; "
        "typedef short int16_t; typedef unsigned short uint16_t; "
        "typedef int int32_t; typedef unsigned int uint32_t; "
        "typedef long long int64_t; typedef unsigned long long uint64_t; ";
  const std::string declaration = is_cxx
      ? "constexpr auto __pcalc_result = (" + input + ");"
      : "static const __typeof__((" + input + ")) __pcalc_result = (" +
            input + ");";
  const std::string source = aliases + declaration;

  clang::TextDiagnosticBuffer diagnostics;
  clang::CompilerInstance compiler;
  if (!parseTranslationUnit(source, is_cxx, diagnostics, compiler)) {
    fail(*result, diagnosticsText(diagnostics));
    return result->status;
  }

  clang::ASTContext &context = compiler.getASTContext();
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
