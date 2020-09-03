#ifndef _PARSE_FUZZ_SPEC_HPP
#define _PARSE_FUZZ_SPEC_HPP

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <map>
#include <iostream>
#include <vector>

#include "globals.hpp"
#include "srcHelperFunctions.hpp"
#include "clang_interface.hpp"
#include "generateMetaTests.hpp"

bool inFuzzTemplate(const clang::Decl*, clang::SourceManager& SM);

struct stmtRedeclTemplateVars
{
    public:
        const clang::Stmt* base_stmt;
        std::vector<clang::SourceRange> output_var_additions;
        std::vector<clang::SourceLocation> decl_var_additions;
        clang::SourceRange output_var_decl;
        std::string output_var_type;

        stmtRedeclTemplateVars(const clang::Stmt* _base_stmt) :
            base_stmt(_base_stmt) {};
};

struct fuzzVarDecl
{
    public:
        const clang::VarDecl* vd;
        std::string name;
        std::string type;
        //clang::QualType type;

        //fuzzVarDecl(std::string _name, std::string _type) :
            //name(_name), type(_type) {};
        fuzzVarDecl(const clang::VarDecl* _vd) :
            vd(_vd), name(vd->getNameAsString()),
            type(vd->getType().getAsString()) {};
        ~fuzzVarDecl() {};

        static bool
        compare(const fuzzVarDecl& lhs, const fuzzVarDecl& rhs)
        {
            return lhs.name.compare(rhs.name) < 0;
        }
};

struct fuzzNewCall
{
    public:
        const clang::Stmt* base_stmt = nullptr;
        const clang::VarDecl* template_var_vd = nullptr;
        const clang::CallExpr* fuzz_call = nullptr;
        const clang::DeclRefExpr* fuzz_ref = nullptr;

        const clang::CallExpr* start_fuzz_call = nullptr;
        const clang::CallExpr* reset_fuzz_call = nullptr;
        bool reset_fuzz_var_decl = false;
};

class fuzzConfigRecorder : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class fuzzConfigParser : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        fuzzConfigRecorder recorder;

    public:

        fuzzConfigParser();

        void
        HandleTranslationUnit(clang::ASTContext& ctx) override;
};

class parseFuzzConfigAction : public clang::ASTFrontendAction
{
    public:
        parseFuzzConfigAction() {}

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef file)
        {
            return std::make_unique<fuzzConfigParser>();
        }
};

class templateVariableDuplicatorVisitor :
    public clang::RecursiveASTVisitor<templateVariableDuplicatorVisitor>
{
    private:
        clang::Rewriter& rw;
        size_t id;

    public:
        templateVariableDuplicatorVisitor(size_t _id, clang::Rewriter& _rw) :
            id(_id), rw(_rw) {};

        std::string getText();
        bool VisitVarDecl(clang::VarDecl*);
        bool VisitDeclRefExpr(clang::DeclRefExpr*);
};

class parseFuzzConstructsVisitor :
    public clang::RecursiveASTVisitor<parseFuzzConstructsVisitor>
{
    private:
        clang::Rewriter& rw;
        clang::ASTContext& ctx;
        bool in_fuzz_template = false;
        bool first_output_var = true;
        const clang::Type* meta_input_var_type = nullptr;
        std::set<std::string> parsed_var_decls;

    public:
        parseFuzzConstructsVisitor(clang::Rewriter& _rw, clang::ASTContext& _ctx) :
            rw(_rw), ctx(_ctx) {};

        const clang::ast_type_traits::DynTypedNode
        getBaseParent(const clang::ast_type_traits::DynTypedNode);

        bool VisitCallExpr(clang::CallExpr*);
        bool VisitVarDecl(clang::VarDecl*);
        bool VisitDeclRefExpr(clang::DeclRefExpr*);
};

class fuzzExpander
{
    public:
        static std::set<std::pair<std::string, std::string>>
        getDuplicateDeclVars(
            //std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)>,
            std::vector<fuzzVarDecl>,
            size_t);

        static void expandLoggedNewVars(clang::Rewriter&, clang::ASTContext&);
        static void expandLoggedNewMRVars(clang::Rewriter&, clang::ASTContext&);
};

class newVariableFuzzerParser : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class newVariableStatementRemover : public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::Rewriter& rw;

    public:
        newVariableStatementRemover(clang::Rewriter& _rw) : rw(_rw) {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class mrNewVariableFuzzerLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        mrNewVariableFuzzerLogger() {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class newVariableFuzzerMatcher : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        newVariableStatementRemover remover;
        newVariableFuzzerParser template_fuzz_var_logger;
        mrNewVariableFuzzerLogger mr_fuzzer_logger;

    public:
        newVariableFuzzerMatcher(clang::Rewriter&);

        void matchAST(clang::ASTContext& ctx)
        {
            matcher.matchAST(ctx);
        }
};

class parseFuzzConstructs : public clang::ASTConsumer
{
    private:
        newVariableFuzzerMatcher newVarFuzzerMtch;
        clang::Rewriter& rw;

    public:
        parseFuzzConstructs(clang::Rewriter& _rw) : rw(_rw),
            newVarFuzzerMtch(newVariableFuzzerMatcher(_rw)) {};

        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            newVarFuzzerMtch.matchAST(ctx);
            fuzzExpander::expandLoggedNewVars(rw, ctx);
            fuzzExpander::expandLoggedNewMRVars(rw, ctx);
        }
};

class templateLocLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::SourceManager& SM;

    public:
        templateLocLogger(clang::SourceManager& _SM) : SM(_SM) {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

/**
 * @brief Duplicates fuzzing template with appropriate variable renaming
 *
 * Every code block between two `fuzz::start()` and `fuzz::end()` calls are
 * duplicated, as they should correspond to a single metamorphic input variable
 * to be generated. First, we identify the correpsonding `clang::CallExpr`s via
 * matchers, then traverse the AST via `parseConstructsVis`.
 *
 */

class templateDuplicator : public clang::ASTConsumer
{
    private:

        /** Matcher callback to log template start/end `CallExpr`s */
        templateLocLogger logger;

        clang::ast_matchers::MatchFinder matcher;
        clang::Rewriter& rw;

    public:
        templateDuplicator(clang::Rewriter&);

        // TODO rather than remove, duplicate
        void HandleTranslationUnit(clang::ASTContext&);
};

class templateDuplicatorAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;

    public:

        bool BeginSourceFileAction(clang::CompilerInstance&) override;
        void EndSourceFileAction() override;

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) override
        {
            rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
            return std::make_unique<templateDuplicator>(rw);
        }
};

/** @brief Action wrapper for parseFuzzConstructs
 *
 * */

class parseFuzzConstructsAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;

    public:
        parseFuzzConstructsAction() {}


        bool BeginSourceFileAction(clang::CompilerInstance&) override;
        void EndSourceFileAction() override;

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) override
        {
            rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
            return std::make_unique<parseFuzzConstructs>(rw);
        }
};

#endif // _PARSE_FUZZ_SPEC_HPP
