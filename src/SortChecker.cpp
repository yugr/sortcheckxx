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
#include "clang/Basic/Version.inc"

#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

// Compatibility layer
#if CLANG_VERSION_MAJOR <= 6
#define getBeginLoc getLocStart
#define getEndLoc getLocEnd
#endif

namespace {

static llvm::cl::OptionCategory Category("SortChecker");
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static llvm::cl::extrahelp MoreHelp("\n\
SortChecker instruments input source files by replacing calls to compare-related APIs\n\
(like std::sort or std::binary_search) with their equivalents that check code for violations\n\
of Strict Weak Ordering axioms at runtime.\n");

llvm::cl::opt<bool> Verbose("v", llvm::cl::desc("Turn on verbose output"));

class Visitor : public RecursiveASTVisitor<Visitor> {
  ASTContext &Ctx;
  Rewriter &RW;
  std::set<FileID> ChangedFiles;

  Expr *skipImplicitCasts(Expr *E) const {
    while (auto *CE = dyn_cast<ImplicitCastExpr>(E)) {
      E = CE->getSubExpr();
    }
    return E;
  }

  void replaceCallee(DeclRefExpr *DRE, llvm::StringRef Replacement) const {
    SourceRange Range = {DRE->getBeginLoc(), DRE->getEndLoc()};
    RW.ReplaceText(Range, Replacement);
  }

  void appendLocationParams(CallExpr *E) const {
    SourceLocation Loc = E->getRParenLoc();
    RW.InsertTextBefore(Loc, ", __FILE__, __LINE__");
  }

  // Locate operator*() in D if it's a CXX class
  CXXMethodDecl *findStarOperator(RecordDecl *D) const {
    if (const auto *RD = dyn_cast<CXXRecordDecl>(D)) {
      for (auto *Method : RD->methods()) {
        if (Method->getOverloadedOperator() == OO_Star) {
          return Method;
        }
      }
    }
    return nullptr;
  }

  QualType canonize(QualType Ty) const {
    return Ty->getCanonicalTypeInternal();
  }

  // Return type of Ty's dereference
  QualType getDereferencedType(QualType Ty) const {
    if (auto *PTy = dyn_cast<PointerType>(Ty.getTypePtr())) {
      return PTy->getPointeeType();
    }

    if (auto *RTy = dyn_cast<RecordType>(Ty.getTypePtr())) {
      if (auto *StarOp = findStarOperator(RTy->getDecl()))
        return canonize(StarOp->getReturnType());
    }

    return QualType();
  }

  bool isBuiltinType(QualType Ty) const {
    Ty = dropReferences(Ty);
    return isa<BuiltinType>(Ty);
  }

  QualType dropReferences(QualType Ty) const {
    while (auto *RTy = dyn_cast<ReferenceType>(Ty.getTypePtr())) {
      Ty = RTy->getPointeeType();
    }
    return Ty;
  }

  bool areTypesCompatible(QualType HaystackTy, QualType NeedleTy) const {
    HaystackTy = dropReferences(HaystackTy);
    NeedleTy = dropReferences(NeedleTy);
    return HaystackTy.getTypePtr() == NeedleTy.getTypePtr();
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

  enum CompareFunction {
    CMP_FUNC_UNKNOWN = 0,
    CMP_FUNC_SORT,
    CMP_FUNC_STABLE_SORT,
    CMP_FUNC_BINARY_SEARCH,
    CMP_FUNC_LOWER_BOUND,
    // TODO: other APIs from
    // https://en.cppreference.com/w/cpp/named_req/Compare
    CMP_FUNC_NUM
  };

  LLVM_NODISCARD bool isKindOfBinarySearch(CompareFunction func) const {
    switch (func) {
    case CMP_FUNC_BINARY_SEARCH:
    case CMP_FUNC_LOWER_BOUND:
      return true;
    default:
      return false;
    }
  }

  CompareFunction getCompareFunction(const std::string &Name) {
    auto F = llvm::StringSwitch<CompareFunction>(Name)
                 .Case("std::sort", CMP_FUNC_SORT)
                 .Case("std::stable_sort", CMP_FUNC_STABLE_SORT)
                 .Case("std::binary_search", CMP_FUNC_BINARY_SEARCH)
                 .Case("std::lower_bound", CMP_FUNC_LOWER_BOUND)
                 .Default(CMP_FUNC_UNKNOWN);
    return F;
  }

  bool VisitCallExpr(CallExpr *E) {
    auto &SM = Ctx.getSourceManager();
    auto Loc = E->getExprLoc();
    if (SM.isInSystemHeader(Loc))
      return true;
    if (!Loc.isValid() || !Loc.isFileID())
      return true;

    auto *Callee = skipImplicitCasts(E->getCallee());
    if (auto *DRE = dyn_cast<DeclRefExpr>(Callee)) {
      std::string S;
      llvm::raw_string_ostream OS(S);
      DRE->getDecl()->printQualifiedName(OS);

      auto LocStr = Loc.printToString(SM);
      if (Verbose) {
        llvm::errs() << "Found call to " << OS.str() << " at " << LocStr
                     << '\n';
      }

      do {
        auto CmpFunc = getCompareFunction(OS.str());
        if (!CmpFunc)
          break;

        if (Verbose) {
          llvm::errs() << "Found relevant function " << OS.str() << "() at "
                       << LocStr << ":\n";
          Callee->dump();
        }

        static struct {
          const char *WrapperName;
          unsigned NumArgs;
        } CompareFunctionInfo[CMP_FUNC_NUM] = {
            {nullptr, 0},
            {"sortcheck::sort_checked", 2},
            {"sortcheck::stable_sort_checked", 2},
            {"sortcheck::binary_search_checked", 3},
            {"sortcheck::lower_bound_checked", 3}};

        std::string WrapperName = CompareFunctionInfo[CmpFunc].WrapperName;

        auto IterTy = canonize(E->getArg(0)->getType());
        auto DerefTy = canonize(getDereferencedType(IterTy));

        const bool HasDefaultCmp =
            E->getNumArgs() == CompareFunctionInfo[CmpFunc].NumArgs;
        const bool IsBuiltinCompare = HasDefaultCmp && isBuiltinType(DerefTy);

        std::optional<bool> CheckRangeFlag;
        if (isKindOfBinarySearch(CmpFunc)) {
          // Enable additional checks if typeof(*__first) == _Tp
          // TODO: iterators must support random access
          auto ValueTy = canonize(E->getArg(2)->getType());
          if (areTypesCompatible(ValueTy, DerefTy)) {
            WrapperName += "_full";
            CheckRangeFlag = !IsBuiltinCompare;
          }
        } else if (IsBuiltinCompare) {
          // Do not instrument std::sort for primitive types
          break;
        }

        replaceCallee(DRE, WrapperName);
        appendLocationParams(E);
        ChangedFiles.insert(SM.getFileID(DRE->getExprLoc()));

        if (CheckRangeFlag) {
          SourceLocation Loc = E->getRParenLoc();
          RW.InsertTextBefore(Loc, *CheckRangeFlag ? ", true" : ", false");
        }
      } while (0);
    }
    return true;
  }

  const std::set<FileID> &getChangedFiles() const { return ChangedFiles; }
};

class Consumer : public ASTConsumer {
public:
  Consumer(CompilerInstance &) {}

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

class InstrumentingAction : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI,
                    LLVM_ATTRIBUTE_UNUSED llvm::StringRef InFile) override {
    CI.getDiagnostics().setClient(new IgnoringDiagConsumer());
    return std::make_unique<Consumer>(CI);
  }

  void PrintHelp(llvm::raw_ostream &ros) { ros << "TODO\n"; }
};

} // namespace

int main(int argc, const char **argv) {
  CommonOptionsParser Op(argc, argv, Category);
  ClangTool Tool(Op.getCompilations(), Op.getSourcePathList());
  return Tool.run(newFrontendActionFactory<InstrumentingAction>().get());
}
