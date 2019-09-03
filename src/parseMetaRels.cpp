#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"
#include <iostream>

#include "libSpecReader.hpp"

static llvm::cl::OptionCategory tmpOC("tmp-cat");

const std::string mtNamespaceName = "meta_tests";
extern std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)> exposed_funcs;

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

    //cTool.run(clang::tooling::newFrontendActionFactory(&finder).get());
    cTool.run(clang::tooling::newFrontendActionFactory<libSpecReaderAction>().get());
    for (ExposedFuncDecl efd : exposed_funcs)
    {
        std::cout << efd.getSignature() << std::endl;
    }

    return 0;
}
