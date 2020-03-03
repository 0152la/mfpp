#ifndef GENERATE_META_TESTS_HPP
#define GENERATE_META_TESTS_HPP

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <sstream>

#include "metaSpecReader.hpp"
#include "clang_interface.hpp"

struct mrGenInfo
{
    mrInfo mr_decl;
    std::string curr_mr_var_name;
    std::vector<std::string> input_var_names;
    size_t depth = 0, test_idx, recursive_idx = 0;
    bool first_decl = true;
    const clang::Rewriter& rw;

    mrGenInfo(mrInfo _mr_decl, std::string _curr_mr_var_name,
        std::vector<std::string> _input_var_names, size_t _test_idx,
        const clang::Rewriter& _rw) :
        mr_decl(_mr_decl), curr_mr_var_name(_curr_mr_var_name),
        input_var_names(_input_var_names), test_idx(_test_idx), rw(_rw) {};

    mrGenInfo(std::vector<std::string> _input_var_names, size_t _test_idx,
        const clang::Rewriter& _rw) :
        mrGenInfo(mrInfo::empty(), "", _input_var_names, _test_idx, _rw) {};

    void setMR(mrInfo, std::string = "");
};

mrInfo retrieveRandMrDecl(std::string, std::string, bool = false);
mrInfo retrieveRandMrDecl(REL_TYPE, std::string, bool = false);

std::string generateMetaTests(std::vector<std::string>, std::string,
    const std::string, clang::Rewriter&);
std::string generateSingleMetaTest(std::vector<std::string>, std::string,
    const std::vector<std::string>&, clang::Rewriter&, size_t);

std::pair<std::string, std::string> concretizeMetaRelation(mrGenInfo&);
std::string makeMRFuncCall(mrGenInfo&, std::vector<std::string> = std::vector<std::string>(), bool = false);
void makeRecursiveFunctionCalls(mrGenInfo&, std::stringstream&);

class testMainLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaCallsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class mrRecursiveLogger: public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        std::map<const clang::FunctionDecl*,
            std::map<const clang::Stmt*, std::vector<const clang::CallExpr*>>>
            matched_recursive_calls;

        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaGenerator : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        clang::ast_matchers::MatchFinder mr_dre_matcher;
        testMainLogger main_logger;
        metaCallsLogger mc_logger;
        metaRelsLogger mr_logger;
        mrDRELogger mr_dre_logger;
        mrRecursiveLogger mr_recursive_logger;
        clang::Rewriter& rw;
        clang::ASTContext& ctx;

    public:
        metaGenerator(clang::Rewriter&, clang::ASTContext&);

        void HandleTranslationUnit(clang::ASTContext&) override;
        void logMetaRelDecl(const clang::FunctionDecl*);
        void expandMetaTests();
};

class metaGeneratorAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;

    public:
        metaGeneratorAction() {};

        bool BeginSourceFileAction(clang::CompilerInstance&) override;
        void EndSourceFileAction() override;

        std::unique_ptr<clang::ASTConsumer>
            CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef)
            override;
};

#endif // GENERATE_META_TESTS_HPP
