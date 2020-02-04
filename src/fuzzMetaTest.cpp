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

#include "generateMetaTests.hpp"
#include "parseFuzzSpec.hpp"
#include "parseFuzzerCalls.hpp"
#include "helperFuncStitch.hpp"
#include "libSpecReader.hpp"

#include "clang_interface.hpp"

static llvm::cl::OptionCategory tmpOC("tmp-cat");
static llvm::cl::opt<size_t> FuzzerSeed("seed",
    llvm::cl::desc("Seed to use in fuzzer"), llvm::cl::init(42),
    llvm::cl::cat(tmpOC));
static llvm::cl::alias FuzzerSeedAlias("s", llvm::cl::aliasopt(FuzzerSeed));
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
    llvm::cl::desc("Comma-separated list of files to expose library functionality."),
    llvm::cl::CommaSeparated, llvm::cl::cat(tmpOC));

size_t meta_input_fuzz_count = 3;
size_t meta_test_rel_count = 7;
llvm::SmallString<256> rewritten_input_file;
std::string rewrite_data;
std::string output_file = "";
std::string set_meta_tests_path = "";

extern std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)> declared_fuzz_vars;
extern std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)>
    exposed_func;

int
main(int argc, char const **argv)
{
    clang::tooling::CommonOptionsParser op(argc, argv, tmpOC);
    fuzzer::clang::setSeed(FuzzerSeed);

    clang::tooling::ClangTool libTool(op.getCompilations(),
        LibInputList);
    clang::tooling::ClangTool metaTool(op.getCompilations(),
        SetMetaTestsInput);
    assert(op.getSourcePathList().size() == 1);
    std::string input_file = op.getSourcePathList().front();
    clang::tooling::ClangTool fuzzTool(op.getCompilations(),
        op.getSourcePathList());

    set_meta_tests_path = SetMetaTestsInput;
    assert(!set_meta_tests_path.empty());
    output_file = TestOutput;

    if (libTool.run(clang::tooling::newFrontendActionFactory<libSpecReaderAction>().get()))
    {
        std::cout << "Error in reading exposed library specification." << std::endl;
        exit(1);
    }
    if (fuzzTool.run(clang::tooling::newFrontendActionFactory<fuzzHelperLoggerAction>().get()))
    {
        std::cout << "Error in logging helper functions." << std::endl;
        exit(1);
    }
    if (fuzzTool.run(clang::tooling::newFrontendActionFactory<templateDuplicatorAction>().get()))
    {
        std::cout << "Error in duplicating fuzzer specifications." << std::endl;
        exit(1);
    }

    std::vector<std::unique_ptr<clang::tooling::FrontendActionFactory>> action_list;
    action_list.emplace_back(
        clang::tooling::newFrontendActionFactory<metaGeneratorAction>());
    action_list.emplace_back(
        clang::tooling::newFrontendActionFactory<parseFuzzConstructsAction>());
    action_list.emplace_back(
        clang::tooling::newFrontendActionFactory<fuzzHelperFuncStitchAction>());
    action_list.emplace_back(
        clang::tooling::newFrontendActionFactory<parseFuzzerCallsAction>());

    size_t step_count = 0;
    for ( std::unique_ptr<clang::tooling::FrontendActionFactory>& fa : action_list)
    {
        step_count += 1;
        clang::tooling::ClangTool processTool(op.getCompilations(),
            std::vector<std::string>{rewritten_input_file.str()});
        if (processTool.run(fa.get()))
        {
            std::cout << "Error in processing step count " << step_count  << std::endl;
            exit(1);
        }
    }
}
