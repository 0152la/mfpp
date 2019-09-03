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
#include <iostream>

#include "parseFuzzSpec.hpp"

static llvm::cl::OptionCategory tmpOC("tmp-cat");

//class replaceInputMetaVarRule final : public clang::tooling::SourceChangeRefactoringRule
//{
    //private:
        //clang::DeclRefExpr input, output;

    //public:
        //replaceInputMetaVarRule(clang::DeclRefExpr& _input, clang::DeclRefExpr& _output) :
            //input(_input), output(_output) {}

        //llvm::Expected<clang::tooling::AtomicChanges>
        //createSourceReplacements(clang::tooling::RefactoringRuleContext &rfCtx) override
        //{
            //clang::tooling::AtomicChange repl(rfCtx.getSources(), input.getBeginLoc());
            //repl.replace(rfCtx.getSources(),
                //clang::CharSourceRange::getCharRange(input.getBeginLoc(), input.getEndLoc()),
                //"here");
            //return {repl};
            ////return llvm::Expected<clang::tooling::AtomicChanges>(repl);
        //}
//};

//class replaceInputMetaVarAction final : public clang::tooling::RefactoringAction
//{
    //llvm::StringRef
    //getCommand() const override
    //{
        //return "meta-in-replace";
    //}

    //llvm::StringRef
    //getDescription() const override
    //{
        //return "Replace specified input variable.";
    //}

    //clang::tooling::RefactoringActionRules
    //createActionRules() const override
    //{
        //clang::tooling::RefactoringActionRules rules;
        //rules.push_back(clang::tooling::createRefactoringActionRule<replaceInputMetaVarRule>());
        //return rules;
    //}
//};

class fnMrInvPrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::FunctionDecl* FD =
                    Result.Nodes.getNodeAs<clang::FunctionDecl>("fnMrInv"))
                FD->dump();
        }
};

class i1DeclPrinter: public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::NamedDecl* ND =
                    Result.Nodes.getNodeAs<clang::NamedDecl>("i1Decl"))
                ND->dump();
        }
};

class mrRetStmtPrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::ReturnStmt* RS =
                    Result.Nodes.getNodeAs<clang::ReturnStmt>("mrRet"))
            {
                RS->dump();
            }
        }
};

class MRInvokePrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::DeclRefExpr* mr_invoke =
                Result.Nodes.getNodeAs<clang::DeclRefExpr>("mrInvoke"))
            {
                mr_invoke->dump();
            }
        }
};


class MRInvokeMatcher : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        MRInvokePrinter printer;

    public:
        MRInvokeMatcher()
        {
            matcher.addMatcher(
                clang::ast_matchers::declRefExpr(
                clang::ast_matchers::to(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::namespaceDecl(
                clang::ast_matchers::hasName(
                "meta_tests"))))))
                   .bind("mrInvoke"), &printer);
        }

        void
        HandleTranslationUnit(clang::ASTContext &ctx) override
        {
            matcher.matchAST(ctx);
        }
};


class ParseMRInvokeAction : public clang::ASTFrontendAction
{
    private:
        MRInvokeMatcher matcher;

    public:
        ParseMRInvokeAction() {}

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI,
            llvm::StringRef file) override {
                return std::make_unique<MRInvokeMatcher>();
            };
};


int
main(int argc, char const **argv)
{
    clang::tooling::CommonOptionsParser op(argc, argv, tmpOC);
    clang::tooling::ClangTool cTool(op.getCompilations(), op.getSourcePathList());

    //std::string mrName = "meta_tests::identity::unite_self";
    //clang::ast_matchers::StatementMatcher returnMatcher =
        //clang::ast_matchers::returnStmt(
            //clang::ast_matchers::hasAncestor(
                //clang::ast_matchers::functionDecl(
                    //clang::ast_matchers::hasName(mrName)))).bind("mrRet");

    //clang::ast_matchers::DeclarationMatcher i1DeclMatcher =
        //clang::ast_matchers::namedDecl(
            //clang::ast_matchers::hasName("i1")).bind("i1Decl");

    //clang::ast_matchers::DeclarationMatcher fnMrInvMatcher =
        //clang::ast_matchers::functionDecl(
            //clang::ast_matchers::hasAncestor(
                //clang::ast_matchers::namespaceDecl(
                //clang::ast_matchers::hasName("meta_tests")))).bind("fnMrInv");

    ////i1DeclPrinter printer;
    //mrRetStmtPrinter printer;
    //clang::ast_matchers::MatchFinder finder;
    ////finder.addMatcher(i1DeclMatcher, &printer);
    //finder.addMatcher(returnMatcher, &printer);

    //cTool.run(clang::tooling::newFrontendActionFactory(&finder).get());
    cTool.run(clang::tooling::newFrontendActionFactory<fuzz_input_parse::parseFuzzConfigAction>().get());
    std::map<std::string, clang::APValue*> config_inputs = fuzz_input_parse::config_inputs;
    return 0;
    cTool.run(clang::tooling::newFrontendActionFactory<ParseMRInvokeAction>().get());
}
