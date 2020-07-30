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

#include <chrono>
#include <iostream>

#include "globals.hpp"
#include "srcHelperFunctions.hpp"

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
static llvm::cl::opt<size_t> MetaInputCount("inputs",
    llvm::cl::desc("Number of metamorphic input variable to create via fuzzing."),
    llvm::cl::init(3), llvm::cl::cat(tmpOC));
static llvm::cl::opt<size_t> MetaTestCount("tests",
    llvm::cl::desc("Number of metamorphic tests to generate."),
    llvm::cl::init(20), llvm::cl::cat(tmpOC));
static llvm::cl::opt<size_t> MetaTestRels("test-size",
    llvm::cl::desc("Number of base metamorphic calls per metamorphic test to generate."),
    llvm::cl::init(5), llvm::cl::cat(tmpOC));
static llvm::cl::opt<size_t> MetaTestDepth("test-depth",
    llvm::cl::desc("Number of maximum recursive calls in a metamorphic test to generate."),
    llvm::cl::init(10), llvm::cl::cat(tmpOC));
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

std::chrono::time_point<std::chrono::system_clock> START_TIME;

llvm::SmallString<256> rewritten_input_file;
std::string rewrite_data;
std::string output_file = "";
std::string set_meta_tests_path = "";

std::string meta_var_name = "r";
size_t meta_input_fuzz_count = 3;
size_t meta_test_rel_count = 7;
size_t meta_test_count = 20;
size_t meta_test_depth = 10;
std::string meta_input_var_prefix = "output_var";

void
EMIT_PASS_DEBUG(const std::string& pass_name, clang::Rewriter& pass_rw)
{
    std::error_code ec;
    int fd;
    llvm::sys::fs::createTemporaryFile("", ".cpp", fd, rewritten_input_file);
    llvm::raw_fd_ostream rif_rfo(fd, true);
    pass_rw.getEditBuffer(pass_rw.getSourceMgr().getMainFileID()).write(rif_rfo);
}

int
main(int argc, char const **argv)
{
    START_TIME = std::chrono::system_clock::now();
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
    //assert(!set_meta_tests_path.empty());
    CHECK_CONDITION(llvm::sys::fs::exists(SetMetaTestsInput),
        "Given input SetMetaTests file does not exist!");
    output_file = TestOutput;

    meta_input_fuzz_count = MetaInputCount;
    meta_test_rel_count = MetaTestRels;
    meta_test_count = MetaTestCount;
    meta_test_depth = MetaTestDepth;

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

    std::chrono::duration<double> from_start = std::chrono::system_clock::now() - START_TIME;
    std::cout << "[" << "\033[1m\033[31m" << from_start.count() << "\033[m" << "]";
    std::cout << " End fuzz test." << std::endl;
}
