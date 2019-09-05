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

#include "clang_interface.hpp"
//namespace fuzz_input_parse
//{

const std::string fuzz_config = "./input/spec_fuzz.hpp";
static std::map<std::string, clang::APValue*> config_inputs;
static std::pair<const clang::CallExpr*, const clang::CallExpr*>
    fuzz_template_bounds(nullptr, nullptr);
//static std::vector<std::pair<const clang::Stmt*, const clang::CallExpr*>>
    //fuzz_new_vars;

struct fuzzVarDecl
{
    public:
        std::string name;
        clang::QualType type;

        fuzzVarDecl(std::string _name, clang::QualType _type) :
            name(_name), type(_type) {};

        static bool
        compare(const fuzzVarDecl& lhs, const fuzzVarDecl& rhs)
        {
            return lhs.name.compare(rhs.name) < 0;
        }

};

struct fuzzNewCall
{
    public:
        const clang::Stmt* base_stmt;
        const clang::CallExpr* fuzz_call;
        const clang::DeclRefExpr* fuzz_ref;
};

static std::vector<fuzzNewCall> fuzz_new_vars;
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
                        //vd->getType()->dump();
                        declared_fuzz_vars.emplace(vd->getNameAsString(), vd->getType());
                        addLibType(vd->getType().getAsString());
                        addLibDeclaredObj(vd->getNameAsString(),
                            vd->getType().getAsString());
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

class newVariableFuzzerExpander
{
    public:
        static void
        expandLoggedNewVars(clang::Rewriter& rw, clang::ASTContext& ctx)
        {
            for (fuzzNewCall fnc : fuzz_new_vars)
            {
                const clang::Stmt* base_stmt = fnc.base_stmt;
                const clang::DeclRefExpr* fuzz_ref = fnc.fuzz_ref;
                const clang::Stmt* fuzz_call = fuzz_ref;
                const llvm::StringRef indent =
                    clang::Lexer::getIndentationForLine(base_stmt->getBeginLoc(),
                        rw.getSourceMgr());
                while (true)
                {
                    auto it = ctx.getParents(*fuzz_call).begin();
                    fuzz_call = it->get<clang::Expr>();
                    if (llvm::dyn_cast<clang::CallExpr>(fuzz_call))
                    {
                        break;
                    }
                }
                assert(fuzz_ref->getNumTemplateArgs() == 1);
                std::pair<std::string, std::string> fuzzer_output =
                    generateObjectInstructions(fuzz_ref->template_arguments()[0]
                        .getArgument().getAsType().getAsString());
                rw.InsertText(base_stmt->getBeginLoc(), fuzzer_output.first + indent.str());
                rw.ReplaceText(clang::SourceRange(fuzz_call->getBeginLoc(),
                    fuzz_call->getEndLoc()), fuzzer_output.second);
            }
        }
};

class newVariableFuzzerParser : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            fuzzNewCall fnc;
            if (const clang::Stmt* s =
                    Result.Nodes.getNodeAs<clang::Stmt>("baseStmt"))
            {
                //s->dump();
                fnc.base_stmt = s;
            }
            //if (const clang::CallExpr* ce =
                    //Result.Nodes.getNodeAs<clang::CallExpr>("fuzzCall"))
            //{
                ////ce->dump();
                //fnc.fuzz_call = ce;
            //}
            if (const clang::DeclRefExpr* dre =
                    Result.Nodes.getNodeAs<clang::DeclRefExpr>("fuzzRef"))
            {
                //dre->dump();
                fnc.fuzz_ref = dre;
            }
            fuzz_new_vars.push_back(fnc);
        }
};

class newVariableFuzzerMatcher : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        newVariableFuzzerParser parser;

    public:
        newVariableFuzzerMatcher()
        {
            matcher.addMatcher(
                clang::ast_matchers::stmt(
                clang::ast_matchers::allOf(
                /* Base stmt two away from main.. */
                clang::ast_matchers::hasParent(
                clang::ast_matchers::stmt(
                clang::ast_matchers::hasParent(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::isMain())))),
                /* .. which contains call to fuzz_new */
                clang::ast_matchers::hasDescendant(
                //clang::ast_matchers::callExpr(
                //clang::ast_matchers::callee(
                clang::ast_matchers::declRefExpr(
                clang::ast_matchers::to(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::fuzz_new"))))
                    .bind("fuzzRef"))) )
                    //.bind("fuzzCall"))))
                    .bind("baseStmt"), &parser);
        }

        void matchAST(clang::ASTContext& ctx)
        {
            matcher.matchAST(ctx);
        }
};

class parseFuzzConstructs : public clang::ASTConsumer
{
    private:
        parseFuzzConstructsVisitor parseConstructsVis;
        gatherDeclaredObjsVisitor gatherDeclObjsVis;
        fuzzFuncExpanderVisitor fuzzFuncExpandVis;
        newVariableFuzzerMatcher newVarFuzzerMtch;
        clang::Rewriter& rw;

    public:
        parseFuzzConstructs(clang::Rewriter& _rw) : parseConstructsVis(_rw),
            rw(_rw) {};

        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            parseConstructsVis.TraverseDecl(ctx.getTranslationUnitDecl());
            gatherDeclObjsVis.TraverseDecl(ctx.getTranslationUnitDecl());
            //fuzzFuncExpandVis.TraverseDecl(ctx.getTranslationUnitDecl());
            newVarFuzzerMtch.matchAST(ctx);
            newVariableFuzzerExpander::expandLoggedNewVars(rw, ctx);
        }
};

class templateLocLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::CallExpr* start_ce =
                    Result.Nodes.getNodeAs<clang::CallExpr>("startTemplate"))
            {
                fuzz_template_bounds.first = start_ce;
            }
            else if (const clang::CallExpr* end_ce =
                    Result.Nodes.getNodeAs<clang::CallExpr>("endTemplate"))
            {
                fuzz_template_bounds.second = end_ce;
            }
        }
};

class templateDuplicator : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        clang::Rewriter& rw;
        templateLocLogger logger;

    public:
        templateDuplicator(clang::Rewriter& _rw) : rw(_rw)
        {
            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::end"))))
                    .bind("endTemplate"), &logger);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::start"))))
                    .bind("startTemplate"), &logger);
        };

        // TODO rather than remove, duplicate
        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);

            rw.RemoveText(
                clang::SourceRange(fuzz_template_bounds.first->getBeginLoc(),
                clang::Lexer::getLocForEndOfToken(
                    fuzz_template_bounds.first->getEndLoc(), 0,
                        rw.getSourceMgr(), rw.getLangOpts())));
            rw.RemoveText(
                clang::SourceRange(fuzz_template_bounds.second->getBeginLoc(),
                clang::Lexer::getLocForEndOfToken(
                    fuzz_template_bounds.second->getEndLoc(), 0,
                        rw.getSourceMgr(), rw.getLangOpts())));

            //llvm::outs() << clang::Lexer::getSourceText(
                //clang::CharSourceRange::getCharRange(
                    //clang::SourceRange(
                    //fuzz_template_bounds.first->getBeginLoc(),
                    ////fuzz_template_bounds.second->getEndLoc())),
                    //clang::Lexer::getLocForEndOfToken(
                        //fuzz_template_bounds.second->getEndLoc(), 0,
                        //rw.getSourceMgr(), rw.getLangOpts()))),
                //rw.getSourceMgr(), rw.getLangOpts()) << '\n';

            //rw.ReplaceText(fuzz_template,
        };
};

class templateDuplicatorAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;

    public:
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
            return std::make_unique<templateDuplicator>(rw);
        }
};

class parseFuzzConstructsAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;
        templateLocLogger tll;

    public:
        parseFuzzConstructsAction() {}

        void
        EndSourceFileAction() override
        {
            rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
                .write(llvm::outs());
        }

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) override
        {
            rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
            return std::make_unique<parseFuzzConstructs>(rw);
        }
};

//} // namespace fuzz_input_parse

#endif // _PARSE_FUZZ_SPEC_HPP
