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

#include <optional>
#include <set>
#include <string>
#include <memory>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

// Compatibility layer
#if CLANG_VERSION_MAJOR <= 6
#define getBeginLoc getLocStart
#define getEndLoc getLocEnd
#endif

#ifndef LLVM_NODISCARD
#define LLVM_NODISCARD [[nodiscard]]
#endif

namespace {

static llvm::cl::OptionCategory Category("SortChecker");
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static llvm::cl::extrahelp MoreHelp("\n\
SortChecker instruments input source files by replacing calls to compare-related APIs\n\
(like std::sort or std::binary_search) with their equivalents that check code for violations\n\
of Strict Weak Ordering axioms at runtime.\n");

llvm::cl::opt<bool> Verbose("v", llvm::cl::desc("Turn on verbose output"));
llvm::cl::opt<bool> IgnoreParseErrors("ignore-parse-errors", llvm::cl::desc("Ignore parser errors"));

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
  CXXMethodDecl *findOperator(CXXRecordDecl *RD,
                              OverloadedOperatorKind OpKind) const {
    for (auto *Method : RD->methods()) {
      if (Method->getOverloadedOperator() == OpKind) {
        return Method;
      }
    }
    for (auto &Base : RD->bases()) {
      // TODO: consider access specifier?
      auto *BaseRD = Base.getType()->getAsCXXRecordDecl();
      if (auto *MD = findOperator(BaseRD, OpKind))
        return MD;
    }
    return nullptr;
  }

  bool isRandomAccessIterator(QualType Ty) const {
    if (isa<PointerType>(Ty.getTypePtr()))
      return true;
    if (auto *RD = Ty->getAsCXXRecordDecl())
      return findOperator(RD, OO_Plus);
    return false;
  }

  QualType canonize(QualType Ty) const {
    return Ty->getCanonicalTypeInternal();
  }

  // Return type of Ty's dereference
  QualType getDereferencedType(QualType Ty) const {
    if (auto *PTy = dyn_cast<PointerType>(Ty.getTypePtr())) {
      return PTy->getPointeeType();
    }

    if (auto *RD = Ty->getAsCXXRecordDecl()) {
      if (auto *StarOp = findOperator(RD, OO_Star))
        return canonize(StarOp->getReturnType());
    }

    return QualType();
  }

  Decl *getDecl(QualType Ty) const {
    if (auto *TypedefTy = dyn_cast<TypedefType>(Ty.getTypePtr()))
      return TypedefTy->getDecl();
    if (auto *TagTy = dyn_cast<TagType>(Ty.getTypePtr()))
      return TagTy->getDecl();
    return nullptr;
  }

  llvm::StringRef getRootNamespace(const Decl *D) const {
    llvm::StringRef RootNSName;
    for (auto *DC = D->getDeclContext(); DC; DC = DC->getParent())
      if (const auto *NS = dyn_cast<NamespaceDecl>(DC);
          NS && NS->getIdentifier())
        RootNSName = NS->getIdentifier()->getName();
    return RootNSName;
  }

  bool isStdType(QualType Ty) const {
    Ty = dropReferences(Ty);
    if (auto *D = getDecl(Ty)) {
      return getRootNamespace(D) == "std";
    }
    return false;
  }

  bool isStdLess(QualType Ty) const {
    if (auto *D = dyn_cast_or_null<NamedDecl>(getDecl(Ty))) {
      std::string S;
      llvm::raw_string_ostream OS(S);
      D->printQualifiedName(OS);
      return OS.str() == "std::less";
    }
    return false;
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

  enum CompareFunction {
    CMP_FUNC_UNKNOWN = 0,
    CMP_FUNC_SORT,
    CMP_FUNC_STABLE_SORT,
    CMP_FUNC_BINARY_SEARCH,
    CMP_FUNC_LOWER_BOUND,
    CMP_FUNC_UPPER_BOUND,
    CMP_FUNC_EQUAL_RANGE,
    CMP_FUNC_MAX_ELEMENT,
    CMP_FUNC_MIN_ELEMENT,
    // TODO: other APIs from
    // https://en.cppreference.com/w/cpp/named_req/Compare
    CMP_FUNC_NUM
  };

  LLVM_NODISCARD bool isKindOfBinarySearch(CompareFunction func) const {
    switch (func) {
    case CMP_FUNC_BINARY_SEARCH:
    case CMP_FUNC_LOWER_BOUND:
    case CMP_FUNC_UPPER_BOUND:
    case CMP_FUNC_EQUAL_RANGE:
      return true;
    default:
      return false;
    }
  }

  LLVM_NODISCARD bool isKindOfMaxElement(CompareFunction func) const {
    switch (func) {
    case CMP_FUNC_MAX_ELEMENT:
    case CMP_FUNC_MIN_ELEMENT:
      return true;
    default:
      return false;
    }
  }

  CompareFunction getCompareFunction(const std::string &Name) {
    return llvm::StringSwitch<CompareFunction>(Name)
        .Case("std::sort", CMP_FUNC_SORT)
        .Case("std::stable_sort", CMP_FUNC_STABLE_SORT)
        .Case("std::binary_search", CMP_FUNC_BINARY_SEARCH)
        .Case("std::lower_bound", CMP_FUNC_LOWER_BOUND)
        .Case("std::upper_bound", CMP_FUNC_UPPER_BOUND)
        .Case("std::equal_range", CMP_FUNC_EQUAL_RANGE)
        .Case("std::max_element", CMP_FUNC_MAX_ELEMENT)
        .Case("std::min_element", CMP_FUNC_MIN_ELEMENT)
        .Default(CMP_FUNC_UNKNOWN);
  }

  bool isBuiltinCompare(QualType KeyTy, bool HasDefaultCmp) const {
    return HasDefaultCmp && (isBuiltinType(KeyTy) || isStdType(KeyTy));
  }

  bool canInstrument(SourceLocation Loc, SourceManager &SM) const {
    if (SM.isInSystemHeader(Loc))
      return false;
    if (!Loc.isValid() || !Loc.isFileID())
      return false;
    return true;
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
    if (!canInstrument(Loc, SM))
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
            {"sortcheck::lower_bound_checked", 3},
            {"sortcheck::upper_bound_checked", 3},
            {"sortcheck::equal_range_checked", 3},
            {"sortcheck::max_element_checked", 2},
            {"sortcheck::min_element_checked", 2}};

        std::string WrapperName = CompareFunctionInfo[CmpFunc].WrapperName;

        auto IterTy = canonize(E->getArg(0)->getType());
        auto DerefTy = canonize(getDereferencedType(IterTy));

        const bool HasDefaultCmp =
            E->getNumArgs() == CompareFunctionInfo[CmpFunc].NumArgs;
        const bool IsBuiltinCompare = isBuiltinCompare(DerefTy, HasDefaultCmp);
        const bool IsRandomAccess = isRandomAccessIterator(IterTy);

        std::optional<bool> CheckRangeFlag;
        if (isKindOfBinarySearch(CmpFunc)) {
          // Enable additional checks if typeof(*__first) == _Tp
          auto ValueTy = canonize(E->getArg(2)->getType());
          if (IsRandomAccess && areTypesCompatible(ValueTy, DerefTy)) {
            WrapperName += "_full";
            CheckRangeFlag = !IsBuiltinCompare;
          }
        } else if (isKindOfMaxElement(CmpFunc)) {
          if (IsBuiltinCompare || !IsRandomAccess)
            break;
        } else if (IsBuiltinCompare) {
          // Do not instrument std::sort for primitive types
          break;
        }

        replaceCallee(DRE, WrapperName);
        appendLocationParams(E);
        ChangedFiles.insert(SM.getFileID(Loc));

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
    if (IgnoreParseErrors)
      CI.getDiagnostics().setClient(new IgnoringDiagConsumer());
    return std::make_unique<Consumer>(CI);
  }

  void PrintHelp(llvm::raw_ostream &OS) { OS << "TODO\n"; }
};

} // namespace

int main(int argc, const char **argv) {
  auto Op = CommonOptionsParser::create(argc, argv, Category, llvm::cl::OneOrMore);
  ClangTool Tool(Op->getCompilations(), Op->getSourcePathList());
  return Tool.run(newFrontendActionFactory<InstrumentingAction>().get());
}
