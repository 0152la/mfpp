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

#include "helperFuncStitch.hpp"
#include "parseFuzzSpec.hpp"
#include "clang_interface.hpp"

enum REL_TYPE
{
    GENERATOR,
    RELATION,
};

class mrInfo: public helperFnDeclareInfo
{
    private:
        REL_TYPE mr_type;
        std::string mr_name;
        std::string mr_family;

    public:
        mrInfo(const clang::FunctionDecl* _fn);

        bool operator<(const mrInfo& other) const
        {
            return this->base_func->getQualifiedNameAsString() <
                other.base_func->getQualifiedNameAsString();
        };

        REL_TYPE getType() const { return this->mr_type; };
        std::string getFamily() const { return this->mr_family; };
};

mrInfo retrieveRandMrDecl(REL_TYPE mr_type, std::string family);
void logMetaRelDecl(const clang::FunctionDecl*);
std::string generateMetaTests(std::vector<std::string>, std::string,
    const std::string);
std::string generateSingleMetaTest(std::vector<std::string>, std::string, const std::vector<std::string>&);
std::string concretizeMetaRelation(const mrInfo&,
    helperFnDeclareInfo, size_t, clang::ASTContext&);


class metaRelsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaGenerator : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        metaRelsLogger logger;
        clang::Rewriter& rw;
        clang::ASTContext& ctx;

    public:
        metaGenerator(clang::Rewriter&, clang::ASTContext&);

        void HandleTranslationUnit(clang::ASTContext&) override;
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
