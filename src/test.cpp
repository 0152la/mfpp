#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"
#include <iostream>

static llvm::cl::OptionCategory tmpOC("tmp-cat");

const std::string mtNamespaceName = "meta_tests";

class mtNsPrinter : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        {
            if (const clang::FunctionDecl* FD =
                    Result.Nodes.getNodeAs<clang::FunctionDecl>("mrDecl"))
                FD->dump();
        }
};

int
main(int argc, char const **argv)
{
    clang::tooling::CommonOptionsParser op(argc, argv, tmpOC);
    clang::tooling::ClangTool cTool(op.getCompilations(), op.getSourcePathList());

    clang::ast_matchers::DeclarationMatcher mrDeclMatcher =
        clang::ast_matchers::functionDecl(
            clang::ast_matchers::hasAncestor(
                clang::ast_matchers::namespaceDecl(
                clang::ast_matchers::hasName(mtNamespaceName)))).bind("mrDecl");

    mtNsPrinter printer;
    clang::ast_matchers::MatchFinder finder;
    finder.addMatcher(mrDeclMatcher, &printer);

    cTool.run(clang::tooling::newFrontendActionFactory(&finder).get());
    return 0;
}
