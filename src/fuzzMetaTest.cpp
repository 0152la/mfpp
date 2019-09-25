#include "clang/AST/Expr.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring/AtomicChange.h"
#include "clang/Tooling/Refactoring/RefactoringAction.h"
#include "clang/Tooling/Refactoring/RefactoringActionRules.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CachePruning.h"
#include <iostream>

#include "parseFuzzSpec.hpp"
#include "libSpecReader.hpp"

static llvm::cl::OptionCategory tmpOC("tmp-cat");
static llvm::cl::opt<std::string> LibInput("lib-files",
    llvm::cl::desc("Files used to expose library functionality for fuzzer."),
    llvm::cl::cat(tmpOC));
static llvm::cl::opt<std::string> SetMetaTestsInput("set-meta-path",
    llvm::cl::desc("Old-YAML format set meta tests spec."),
    llvm::cl::cat(tmpOC));
static llvm::cl::opt<std::string> TestOutput("output",
    llvm::cl::desc("Path where to emit output file."),
    llvm::cl::cat(tmpOC), llvm::cl::Required, llvm::cl::value_desc("filename"));
static llvm::cl::alias TestOutputAlias("o",
    llvm::cl::desc("Path where to emit output file."),
    llvm::cl::aliasopt(TestOutput));
static llvm::cl::list<std::string> LibInputList("lib-list",
    llvm::cl::CommaSeparated, llvm::cl::cat(tmpOC));

size_t meta_input_fuzz_count = 5;
llvm::SmallString<256> rewritten_input_file;
std::string output_file = "";
std::string meta_input_var_type = "";
std::string set_meta_tests_path = "";

extern std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)> declared_fuzz_vars;
extern std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)>
    exposed_func;

class fnMrInvPrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::FunctionDecl* FD =
                    Result.Nodes.getNodeAs<clang::FunctionDecl>("fnMrInv"))
                FD->dump();
        }
};

class i1DeclPrinter: public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::NamedDecl* ND =
                    Result.Nodes.getNodeAs<clang::NamedDecl>("i1Decl"))
                ND->dump();
        }
};

class mrRetStmtPrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::ReturnStmt* RS =
                    Result.Nodes.getNodeAs<clang::ReturnStmt>("mrRet"))
            {
                RS->dump();
            }
        }
};

class MRInvokePrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::DeclRefExpr* mr_invoke =
                Result.Nodes.getNodeAs<clang::DeclRefExpr>("mrInvoke"))
            {
                mr_invoke->dump();
            }
        }
};


class MRInvokeMatcher : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        MRInvokePrinter printer;

    public:
        MRInvokeMatcher()
        {
            matcher.addMatcher(
                clang::ast_matchers::declRefExpr(
                clang::ast_matchers::to(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::namespaceDecl(
                clang::ast_matchers::hasName(
                "meta_tests"))))))
                   .bind("mrInvoke"), &printer);
        }

        void
        HandleTranslationUnit(clang::ASTContext &ctx) override
        {
            matcher.matchAST(ctx);
        }
};


class ParseMRInvokeAction : public clang::ASTFrontendAction
{
    private:
        MRInvokeMatcher matcher;

    public:
        ParseMRInvokeAction() {}

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI,
            llvm::StringRef file) override {
                return std::make_unique<MRInvokeMatcher>();
            };
};


int
main(int argc, char const **argv)
{
    clang::tooling::CommonOptionsParser op(argc, argv, tmpOC);

    std::vector<std::string> lib_source_path_list {LibInput};
    clang::tooling::ClangTool libTool(op.getCompilations(),
        //std::vector<std::string>{LibInput});
        LibInputList);
    assert(op.getSourcePathList().size() == 1);
    std::string input_file = op.getSourcePathList().front();
    clang::tooling::ClangTool fuzzTool(op.getCompilations(),
        op.getSourcePathList());

    set_meta_tests_path = SetMetaTestsInput;
    assert(!set_meta_tests_path.empty());
    output_file = TestOutput;

    //cTool.run(clang::tooling::newFrontendActionFactory<fuzz_input_parse::parseFuzzConfigAction>().get());
    //std::map<std::string, clang::APValue*> config_inputs = fuzz_input_parse::config_inputs;
    libTool.run(clang::tooling::newFrontendActionFactory<libSpecReaderAction>().get());
    fuzzTool.run(clang::tooling::newFrontendActionFactory<templateDuplicatorAction>().get());

    clang::tooling::ClangTool fuzz2Tool(op.getCompilations(),
        std::vector<std::string>{rewritten_input_file.str()});
    //llvm::StringRef if_sref(input_file), rif_sref(rewritten_input_file);
    //fuzz2Tool.mapVirtualFile(if_sref, rif_sref);
    fuzz2Tool.run(clang::tooling::newFrontendActionFactory<parseFuzzConstructsAction>().get());
}
