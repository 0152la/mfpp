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

mrInfo retrieveRandMrDecl(REL_TYPE mr_type, std::string family);
std::string generateMetaTests(std::vector<std::string>, std::string,
    const std::string, clang::Rewriter&);
std::string generateSingleMetaTest(std::vector<std::string>, std::string,
    const std::vector<std::string>&, clang::Rewriter&);
std::pair<std::string, std::string> concretizeMetaRelation(helperFnDeclareInfo,
    size_t, clang::Rewriter&);

class testMatcherCallback: public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result) override
        {
            std::cout << "CALLBACK MATCH" << std::endl;
            const clang::DeclRefExpr* FD = Result.Nodes.getNodeAs<clang::DeclRefExpr>("fdTest");
            assert(FD);
            FD->dump();
        };
};

class metaCallsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaGenerator : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        clang::ast_matchers::MatchFinder mr_dre_matcher;
        metaCallsLogger mc_logger;
        metaRelsLogger mr_logger;
        mrDRELogger mr_dre_logger;
        testMatcherCallback test_mcb;
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
