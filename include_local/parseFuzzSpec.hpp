#ifndef _PARSE_FUZZ_SPEC_HPP
#define _PARSE_FUZZ_SPEC_HPP

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <map>
#include <iostream>

//namespace fuzz_input_parse
//{

const std::string fuzz_config = "./input/spec_fuzz.hpp";
static std::map<std::string, clang::APValue*> config_inputs;
static clang::SourceLocation template_start, template_end;

class fuzzVarDecl
{
    private:
        std::string name;
        clang::QualType type;

    public:
        fuzzVarDecl(std::string _name, clang::QualType _type) :
            name(_name), type(_type) {};

        static bool
        compare(const fuzzVarDecl& lhs, const fuzzVarDecl& rhs)
        {
            return lhs.name.compare(rhs.name) < 0;
        }

};

static std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)>
    declared_fuzz_vars(&fuzzVarDecl::compare);

class fuzzConfigRecorder : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::VarDecl* VD =
                    Result.Nodes.getNodeAs<clang::VarDecl>("inputDecl"))
            {
                config_inputs.insert(std::make_pair(VD->getNameAsString(), VD->evaluateValue()));
            }
        }
};

class fuzzConfigParser : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        fuzzConfigRecorder recorder;

    public:
        fuzzConfigParser()
        {
            matcher.addMatcher(
                clang::ast_matchers::varDecl(
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::namespaceDecl(
                clang::ast_matchers::hasName(
                "fuzz::input"))))
                    .bind("inputDecl"), &recorder);
        }

        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);
        }
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

class parseFuzzConstructsVisitor :
    public clang::RecursiveASTVisitor<parseFuzzConstructsVisitor>
{
    private:
        clang::Rewriter& rw;

    public:
        parseFuzzConstructsVisitor(clang::Rewriter& _rw) : rw(_rw) {};

        bool
        VisitDeclRefExpr(clang::DeclRefExpr* dre)
        {
            //clang::NamedDecl* nd = dre->getFoundDecl();
            //if (nd->getNameAsString().compare("output_var"))
            //{
                //return true;
            //}
            if (dre->hasQualifier())
            {
                clang::NamespaceDecl* nd = dre->getQualifier()->getAsNamespace();
                if (nd && !nd->getNameAsString().compare("fuzz"))
                {
                    if (!dre->getDecl()->getNameAsString().compare("output_var"))
                    {
                        rw.ReplaceText(clang::SourceRange(dre->getBeginLoc(), dre->getEndLoc()),
                            llvm::StringRef("output_0"));
                         //rw.InsertText(dre->getBeginLoc(), "test");
                    }
                }
            }
            return true;
        }
};

class gatherDeclaredObjsVisitor :
    public clang::RecursiveASTVisitor<gatherDeclaredObjsVisitor>
{
    public:
        bool
        VisitFunctionDecl(clang::FunctionDecl* fd)
        {
            if (fd->isMain())
            {
                for (clang::Decl* d : fd->decls())
                {
                    if (clang::VarDecl* vd = llvm::dyn_cast<clang::VarDecl>(d);
                        vd && !llvm::dyn_cast<clang::ParmVarDecl>(vd))
                    {
                        declared_fuzz_vars.emplace(vd->getNameAsString(), vd->getType());
                    }
                }
            }
            return true;
        }
};

class fuzzFuncExpanderVisitor :
    public clang::RecursiveASTVisitor<fuzzFuncExpanderVisitor>
{
    public:
        bool
        VisitDeclRefExpr(clang::DeclRefExpr* dre)
        {
            if (clang::NamespaceDecl* nd = dre->hasQualifier()
                    ? dre->getQualifier()->getAsNamespace()
                    : nullptr;
                nd && !nd->getNameAsString().compare("fuzz"))
            {
                if (clang::FunctionDecl* fd = llvm::dyn_cast<clang::FunctionDecl>(dre->getDecl()))
                {
                    if (fd->getName().equals("meta_test"))
                    {
                        fd->dump();
                    }
                    else if (fd->getName().equals("fuzz_new"))
                    {
                    }
                    else
                    {
                        std::cout << "Not implemented fuzzing call ";
                        std::cout << fd->getNameAsString() << std::endl;
                        exit(0);
                    }
                }
            }
            return true;
        }
};

class parseFuzzConstructs : public clang::ASTConsumer
{
    private:
        parseFuzzConstructsVisitor parseConstructVis;
        gatherDeclaredObjsVisitor gatherDeclObjsVis;
        fuzzFuncExpanderVisitor fuzzFuncExpandVis;

    public:
        parseFuzzConstructs(clang::Rewriter& _rw) : parseConstructVis(_rw) {};

        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            //parseConstructsVis.TraverseDecl(ctx.getTranslationUnitDecl());
            //gatherDeclObjsVis.TraverseDecl(ctx.getTranslationUnitDecl());
            fuzzFuncExpandVis.TraverseDecl(ctx.getTranslationUnitDecl());
        }
};

class templateLocLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            std::cout << "SET TRY" << std::endl;
            if (const clang::DeclRefExpr* start_dre =
                    Result.Nodes.getNodeAs<clang::DeclRefExpr>("startTemplate"))
            {
                template_start = start_dre->getEndLoc();
                std::cout << "SET BEGIN" << std::endl;
            }
            else if (const clang::DeclRefExpr* end_dre =
                    Result.Nodes.getNodeAs<clang::DeclRefExpr>("endTemplate"))
            {
                template_end = end_dre->getBeginLoc();
                std::cout << "SET END" << std::endl;
            }
        }
};

class parseFuzzConstructsAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;
        templateLocLogger tll;

    public:
        parseFuzzConstructsAction() {}

        bool
        BeginSourceFileAction(clang::CompilerInstance& ci) override
        {
            return true;
        }

        void
        EndSourceFileAction() override
        {
            //rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
                //.write(llvm::outs());
        }

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) override
        {
            rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());

            clang::ast_matchers::MatchFinder meta_in_template_matcher;
            meta_in_template_matcher.addMatcher(
                clang::ast_matchers::declRefExpr(
                clang::ast_matchers::to(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::end"))))
                    .bind("endTemplate"), &tll);

            meta_in_template_matcher.addMatcher(
                clang::ast_matchers::declRefExpr(
                clang::ast_matchers::to(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::start"))))
                    .bind("startTemplate"), &tll);

            meta_in_template_matcher.matchAST(ci.getASTContext());
            std::cout << template_start.printToString(ci.getSourceManager()) << std::endl;
            std::cout << template_end.printToString(ci.getSourceManager()) << std::endl;
            clang::SourceRange fuzz_template(template_start, template_end);
            std::cout << fuzz_template.printToString(ci.getSourceManager()) << std::endl;
            exit(1);
            //rw.ReplaceText(clang::SourceRange(template_start, template_end),
                //"test")

            return std::make_unique<parseFuzzConstructs>(rw);
        }
};

//} // namespace fuzz_input_parse

#endif // _PARSE_FUZZ_SPEC_HPP
