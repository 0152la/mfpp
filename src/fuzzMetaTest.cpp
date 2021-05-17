 /******************************************************************************
 *  Copyright 2021 Andrei Lascu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

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

static llvm::cl::OptionCategory fuzzMetaTest("fuzz-meta-test");
static llvm::cl::opt<size_t> FuzzerSeed("seed",
    llvm::cl::desc("Seed to use in fuzzer"), llvm::cl::init(42),
    llvm::cl::cat(fuzzMetaTest));
static llvm::cl::alias FuzzerSeedAlias("s", llvm::cl::aliasopt(FuzzerSeed));
static llvm::cl::opt<size_t> MetaInputCount("inputs",
    llvm::cl::desc("Number of metamorphic input variable to create via fuzzing."),
    llvm::cl::init(3), llvm::cl::cat(fuzzMetaTest));
static llvm::cl::opt<size_t> MetaTestCount("tests",
    llvm::cl::desc("Number of metamorphic tests to generate."),
    llvm::cl::init(20), llvm::cl::cat(fuzzMetaTest));
static llvm::cl::opt<size_t> MetaTestRels("test-size",
    llvm::cl::desc("Number of base metamorphic calls per metamorphic test to generate."),
    llvm::cl::init(5), llvm::cl::cat(fuzzMetaTest));
static llvm::cl::opt<size_t> MetaTestDepth("test-depth",
    llvm::cl::desc("Number of maximum recursive calls in a metamorphic test to generate."),
    llvm::cl::init(10), llvm::cl::cat(fuzzMetaTest));
static llvm::cl::opt<std::string> TestOutput("output",
    llvm::cl::desc("Path where to emit output file."),
    llvm::cl::cat(fuzzMetaTest), llvm::cl::Required, llvm::cl::value_desc("filename"));
static llvm::cl::alias TestOutputAlias("o",
    llvm::cl::desc("Path where to emit output file."),
    llvm::cl::aliasopt(TestOutput), llvm::cl::cat(fuzzMetaTest));
static llvm::cl::opt<bool> TrivialVariantCheck("trivial-check",
    llvm::cl::desc("Whether to trivially check that the first produced variant is equivalent to itself."),
    llvm::cl::cat(fuzzMetaTest));
static llvm::cl::opt<depth_pruning> DepthPruning("prune-depth",
    llvm::cl::desc("Algorithm to use to prune recursive depth."),
    llvm::cl::values(
        clEnumVal(noprune, "[DEFAULT] Unbound recursion - only stops recursing once depth limit hit."),
        clEnumVal(linear, "Linearly increase probability of pruning recursion with depth increase."),
        clEnumVal(logarithm, "Quickly increase probability to prune recursion early on.")),
    llvm::cl::init(depth_pruning::noprune), llvm::cl::cat(fuzzMetaTest));

std::chrono::time_point<std::chrono::system_clock> globals::START_TIME;

llvm::SmallString<256> globals::rewritten_input_file;
std::string globals::rewrite_data;
std::string globals::output_file = "";

size_t globals::meta_input_fuzz_count = 3;
size_t globals::meta_test_rel_count = 7;
size_t globals::meta_test_count = 20;
size_t globals::meta_test_depth = 10;
bool globals::trivial_check;
depth_pruning globals::prune_option;

void
EMIT_PASS_DEBUG(const std::string& pass_name, clang::Rewriter& pass_rw)
{
    std::error_code ec;
    int fd;
    llvm::sys::fs::createTemporaryFile("", ".cpp", fd, globals::rewritten_input_file);
    llvm::raw_fd_ostream rif_rfo(fd, true);
    pass_rw.getEditBuffer(pass_rw.getSourceMgr().getMainFileID()).write(rif_rfo);
}

int
main(int argc, const char **argv)
{
    globals::START_TIME = std::chrono::system_clock::now();
    clang::tooling::CommonOptionsParser op(argc, argv, fuzzMetaTest);
    fuzzer::clang::setSeed(FuzzerSeed);
    std::cout << "[fuzzMetaTest] Seed set " << FuzzerSeed << std::endl;

    assert(op.getSourcePathList().size() == 1);
    std::string input_file = op.getSourcePathList().front();
    clang::tooling::ClangTool fuzzTool(op.getCompilations(),
        op.getSourcePathList());

    globals::output_file = TestOutput;

    globals::meta_input_fuzz_count = MetaInputCount;
    globals::meta_test_rel_count = MetaTestRels;
    globals::meta_test_count = MetaTestCount;
    globals::meta_test_depth = MetaTestDepth;
    globals::trivial_check = TrivialVariantCheck;
    globals::prune_option = DepthPruning;

    if (fuzzTool.run(clang::tooling::newFrontendActionFactory<libSpecReaderAction>().get()))
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
            std::vector<std::string>{globals::rewritten_input_file.str()});
        if (processTool.run(fa.get()))
        {
            std::cout << "Error in processing step count " << step_count  << std::endl;
            exit(1);
        }
    }

    std::chrono::duration<double> from_start = std::chrono::system_clock::now() - globals::START_TIME;
    std::cout << "[" << "\033[1m\033[31m" << from_start.count() << "\033[m" << "]";
    std::cout << " End fuzz test " << TestOutput << "." << std::endl;
}
