// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "clang/Rewrite/Core/Rewriter.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"

#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

namespace {

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

int verbose = 0;

class Visitor : public RecursiveASTVisitor<Visitor> {
  ASTContext &Ctx;
  Rewriter &RW;
  std::set<FileID> ChangedFiles;

  Expr *skipImplicitCasts(Expr *E) {
    while (auto *CE = dyn_cast<ImplicitCastExpr>(E)) {
      E = CE->getSubExpr();
    }
    return E;
  }

  void replaceCallee(DeclRefExpr *DRE, const char *Replacement) {
    SourceRange Range = {DRE->getBeginLoc(), DRE->getEndLoc()};
    RW.ReplaceText(Range, Replacement);
  }

  void appendLocationParams(CallExpr *E) {
    SourceLocation Loc = E->getRParenLoc();
    RW.InsertTextBefore(Loc, ", __FILE__, __LINE__");
  }

public:
  Visitor(ASTContext &Ctx, Rewriter &RW) : Ctx(Ctx), RW(RW) {}

#if 0
  bool VisitExpr(Expr *E) {
    llvm::errs() << "Expr at ";
    E->getExprLoc().dump(Ctx.getSourceManager());
    E->dump();
    return true;
  }
#endif

  bool VisitCallExpr(CallExpr *E) {
    auto &SM = Ctx.getSourceManager();
    auto Loc = E->getExprLoc();
    if (SM.isInSystemHeader(Loc))
      return true;;

    auto *Callee = skipImplicitCasts(E->getCallee());
    if (auto *DRE = dyn_cast<DeclRefExpr>(Callee)) {
      std::string S;
      llvm::raw_string_ostream OS(S);
      DRE->getDecl()->printQualifiedName(OS);

      auto LocStr = Loc.printToString(SM);
      if (verbose) {
        llvm::errs() << "Found call to " << OS.str() << " at " << LocStr << '\n';
      }

      // TODO: other APIs from https://en.cppreference.com/w/cpp/named_req/Compare
      // TODO: skip primitive types
      if (OS.str() == "std::sort" || OS.str() == "std::bsearch") {
        if (verbose) {
          llvm::errs() << "Found " << OS.str() << "() at " << LocStr << ":\n";
          Callee->dump();
        }
        auto Replacement =
            OS.str() == "std::sort" ? "sortcheck::sort_checked" : "sortcheck::bsearch_checked";
        replaceCallee(DRE, Replacement);
        appendLocationParams(E);
        ChangedFiles.insert(SM.getFileID(DRE->getExprLoc()));
      }
    }
    return true;
  }

  const std::set<FileID> &getChangedFiles() const {
    return ChangedFiles;
  }
};

class Consumer : public ASTConsumer {
  CompilerInstance &CI;

public:
  Consumer(CompilerInstance &CI) : CI(CI) {}

  void HandleTranslationUnit(ASTContext &Ctx) override {
    auto &SM = Ctx.getSourceManager();
    Rewriter RW(SM, Ctx.getLangOpts());

    // Replace calls
    Visitor V(Ctx, RW);
    V.TraverseDecl(Ctx.getTranslationUnitDecl());

    // Insert includes
    for (auto &FID : V.getChangedFiles()) {
      auto Loc = SM.getLocForStartOfFile(FID);
      RW.InsertText(Loc, "#include <sortcheck.h>\n");
    }

    // Modify files
    RW.overwriteChangedFiles();
  }
};

class Action : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(
    CompilerInstance &CI,
    LLVM_ATTRIBUTE_UNUSED llvm::StringRef InFile) override {
    CI.getDiagnostics().setClient(new IgnoringDiagConsumer());
    return std::make_unique<Consumer>(CI);
  }

  void PrintHelp(llvm::raw_ostream &ros) {
    ros << "TODO\n";
  }
};

} // anon namespace

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());
  return Tool.run(newFrontendActionFactory<Action>().get());
}
