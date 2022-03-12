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
    CMP_FUNC_UPPER_BOUND,
    // TODO: other APIs from
    // https://en.cppreference.com/w/cpp/named_req/Compare
    CMP_FUNC_NUM
  };

  enum Container {
    CONTAINER_UNKNOWN = 0,
    CONTAINER_SET,
    CONTAINER_MAP,
    CONTAINER_MULTISET,
    CONTAINER_MULTIMAP,
    CONTAINER_NUM
  };

  LLVM_NODISCARD bool isKindOfBinarySearch(CompareFunction func) const {
    switch (func) {
    case CMP_FUNC_BINARY_SEARCH:
    case CMP_FUNC_LOWER_BOUND:
    case CMP_FUNC_UPPER_BOUND:
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
               .Default(CMP_FUNC_UNKNOWN);
  }

  Container getContainerType(const RecordDecl *D) const {
    if (getRootNamespace(D) != "std")
      return CONTAINER_UNKNOWN;
    auto Name = D->getName();
    return llvm::StringSwitch<Container>(Name)
        .Case("map", CONTAINER_MAP)
        .Case("set", CONTAINER_SET)
        .Case("multimap", CONTAINER_MULTIMAP)
        .Case("multiset", CONTAINER_MULTISET)
        .Default(CONTAINER_UNKNOWN);
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
        const bool IsBuiltinCompare =
            HasDefaultCmp && (isBuiltinType(DerefTy) || isStdType(DerefTy));

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
        ChangedFiles.insert(SM.getFileID(Loc));

        if (CheckRangeFlag) {
          SourceLocation Loc = E->getRParenLoc();
          RW.InsertTextBefore(Loc, *CheckRangeFlag ? ", true" : ", false");
        }
      } while (0);
    } else if (const auto *CE = dyn_cast<CXXMemberCallExpr>(E)) {
      do {
        const auto *RD = CE->getRecordDecl();
        auto Cont = getContainerType(RD);
        if (!Cont)
          break;

        const auto *MD = CE->getMethodDecl();
        if (MD->getName() != "clear")
          break;

        auto LocStr = Loc.printToString(SM);
        if (Verbose) {
          llvm::errs() << "Found relevant function " << MD->getName() << "() at "
                       << LocStr << ":\n";
          Callee->dump();
        }

        static struct {
          const char *Name;
          unsigned CompareArg;
        } ContainerInfo[CONTAINER_NUM] = {
            {nullptr, 0},
            {"set", 1},
            {"map", 2},
            {"multiset", 1},
            {"multimap", 2}};

        auto CompareArg = ContainerInfo[Cont].CompareArg;

        if (auto *SpecDecl = dyn_cast<ClassTemplateSpecializationDecl>(RD)) {
          auto &ArgList = SpecDecl->getTemplateArgs();

          auto KeyTy = ArgList.get(0).getAsType();
          auto CmpTy = ArgList.get(CompareArg).getAsType();

          const bool HasDefaultCmp = isStdLess(CmpTy);
          const bool IsBuiltinCompare =
            HasDefaultCmp && (isBuiltinType(KeyTy) || isStdType(KeyTy));

          if (IsBuiltinCompare)
            break;
        }

        auto *This = CE->getImplicitObjectArgument();
        RW.InsertTextBefore(This->getBeginLoc(), "sortcheck::check_associative(");
        auto EndLoc = Lexer::getLocForEndOfToken(This->getEndLoc(), 0, SM, Ctx.getLangOpts());
        RW.InsertTextAfter(EndLoc, ", __FILE__, __LINE__)");
        ChangedFiles.insert(SM.getFileID(Loc));
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

  void PrintHelp(llvm::raw_ostream &OS) { OS << "TODO\n"; }
};

} // namespace

int main(int argc, const char **argv) {
  CommonOptionsParser Op(argc, argv, Category);
  ClangTool Tool(Op.getCompilations(), Op.getSourcePathList());
  return Tool.run(newFrontendActionFactory<InstrumentingAction>().get());
}
