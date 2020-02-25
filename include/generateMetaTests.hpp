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

mrInfo retrieveRandMrDecl(std::string mr_type, std::string family);
mrInfo retrieveRandMrDecl(REL_TYPE mr_type, std::string family);
std::string generateMetaTests(std::vector<std::string>, std::string,
    const std::string, clang::Rewriter&);
std::string generateSingleMetaTest(std::vector<std::string>, std::string,
    const std::vector<std::string>&, clang::Rewriter&, size_t);
std::pair<std::string, std::string> concretizeMetaRelation(mrInfo,
    std::vector<std::string>&, clang::Rewriter&, std::string, bool, size_t, size_t&);
std::string generateRecursiveMRChain(const mrInfo&, std::stringstream&, size_t, size_t, clang::Rewriter&);
std::string makeMRFuncCall(mrInfo, size_t, size_t, std::vector<std::string>&, bool = false);
void makeRecursiveFunctionCalls(mrInfo, clang::Rewriter&, std::stringstream&, size_t, size_t&, std::vector<std::string>);

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
