#ifndef META_SPEC_READER_HPP
#define META_SPEC_READER_HPP

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <sstream>

#include "helperFuncStitch.hpp"

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

class metaRelsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaRelsReader : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        metaRelsLogger logger;

    public:
        metaRelsReader();

        void HandleTranslationUnit(clang::ASTContext&) override;
        static void logMetaRelDecl(const clang::FunctionDecl*);
};

class metaRelsReaderAction : public clang::ASTFrontendAction
{
    public:
        metaRelsReaderAction() {};

        bool BeginSourceFileAction(clang::CompilerInstance&) override;

        std::unique_ptr<clang::ASTConsumer>
            CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef)
            override;
};


#endif // META_SPEC_READER_HPP
