#ifndef _PARSE_FUZZ_RAND_HPP
#define _PARSE_FUZZ_RAND_HPP

#include "clang_interface.hpp"

class fuzzerCallsReplacer
{
    private:
        clang::ASTContext& ctx;
        clang::Rewriter& rw;

    public:
        fuzzerCallsReplacer(clang::ASTContext& _ctx, clang::Rewriter& _rw) :
            ctx(_ctx), rw(_rw) {}

        //void makeReplace(std::vector<helperFnReplaceInfo>&) const;
};

class fuzzerCallsLocator: public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::ASTContext& ctx;

    public:
        fuzzerCallsLocator(clang::ASTContext& _ctx) : ctx(_ctx) {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class parseFuzzerCalls: public clang::ASTConsumer
{
    private:
        fuzzerCallsLocator locator;
        fuzzerCallsReplacer replacer;

        clang::ast_matchers::MatchFinder matcher;
        clang::Rewriter& rw;

    public:
        parseFuzzerCalls(clang::Rewriter&, clang::ASTContext&);
        void HandleTranslationUnit(clang::ASTContext&);
};

class parseFuzzerCallsAction : public clang::ASTFrontendAction
{
    public:
        parseFuzzerCallsAction() {}

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) override;
};

#endif // _PARSE_FUZZ_RAND_HPP
