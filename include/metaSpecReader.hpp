#ifndef META_SPEC_READER_HPP
#define META_SPEC_READER_HPP

#include "clang/AST/RecursiveASTVisitor.h"
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

class mrDRELogger: public clang::ast_matchers::MatchFinder::MatchCallback
{

    public:
        std::vector<const clang::DeclRefExpr*> matched_dres;
        std::vector<const clang::VarDecl*> matched_vds;

        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaRelsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        std::vector<const clang::FunctionDecl*> matched_fds;

        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaRelsReader : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        clang::ast_matchers::MatchFinder mr_dre_matcher;
        metaRelsLogger mr_logger;
        mrDRELogger dre_logger;
        clang::ASTContext& ctx;

    public:
        metaRelsReader(clang::ASTContext& _ctx);

        void HandleTranslationUnit(clang::ASTContext&) override;
        void logMetaRelDecl(const clang::FunctionDecl*);
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
