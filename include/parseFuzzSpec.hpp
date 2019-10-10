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

static std::map<std::string, clang::APValue*> config_inputs;
static std::pair<const clang::CallExpr*, const clang::CallExpr*>
    fuzz_template_bounds(nullptr, nullptr);
static std::set<clang::VarDecl*> input_template_var_decls;
static std::map<size_t, std::vector<std::string>>
    input_template_copies;
static const clang::CompoundStmt* main_child;
static std::vector<const clang::CallExpr*> meta_test_calls;

extern size_t meta_input_fuzz_count;
extern size_t meta_test_rel_count;
extern llvm::SmallString<256> rewritten_input_file;
extern std::string rewrite_data;
extern std::string meta_input_var_type;
extern std::string set_meta_tests_path;

bool
inFuzzTemplate(const clang::Decl* d, clang::SourceManager& SM)
{
    if (!fuzz_template_bounds.first || !fuzz_template_bounds.second)
    {
        return false;
    }
    clang::BeforeThanCompare<clang::SourceLocation> btc(SM);
    return btc(d->getBeginLoc(), fuzz_template_bounds.second->getEndLoc()) &&
        btc(fuzz_template_bounds.first->getBeginLoc(), d->getEndLoc());
}

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
        std::string name;
        std::string type;
        //clang::QualType type;

        fuzzVarDecl(std::string _name, std::string _type) :
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
        const clang::Stmt* base_stmt = nullptr;
        const clang::CallExpr* fuzz_call = nullptr;
        const clang::DeclRefExpr* fuzz_ref = nullptr;

        const clang::CallExpr* start_fuzz_call = nullptr;
        const clang::CallExpr* reset_fuzz_call = nullptr;
        bool reset_fuzz_var_decl = false;
};

static std::vector<fuzzNewCall> fuzz_new_vars;
static std::vector<stmtRedeclTemplateVars> stmt_rewrite_map;
std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)>
    declared_fuzz_vars(&fuzzVarDecl::compare);
static std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)>
    common_template_var_decls(&fuzzVarDecl::compare);

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

class templateVariableDuplicatorVisitor :
    public clang::RecursiveASTVisitor<templateVariableDuplicatorVisitor>
{
    private:
        clang::Rewriter& rw;
        size_t id;

    public:
        templateVariableDuplicatorVisitor(size_t _id, clang::Rewriter& _rw) :
            id(_id), rw(_rw) {};

        std::string
        getText()
        {
            std::string ss_str;
            llvm::raw_string_ostream ss(ss_str);
            rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(ss);
            return ss.str();
        }

        bool
        VisitVarDecl(clang::VarDecl* vd)
        {
            rw.InsertText(vd->getLocation().getLocWithOffset(vd->getName().size()),
                "_" + std::to_string(id));
            return true;
        }

        bool
        VisitDeclRefExpr(clang::DeclRefExpr* dre)
        {
            if (llvm::dyn_cast<clang::VarDecl>(dre->getDecl()))
            {
                rw.InsertText(dre->getEndLoc(), "_" + std::to_string(id));
            }
            return true;
        }
};

class parseFuzzConstructsVisitor :
    public clang::RecursiveASTVisitor<parseFuzzConstructsVisitor>
{
    private:
        clang::Rewriter& rw;
        clang::ASTContext& ctx;
        bool in_fuzz_template = false;
        bool first_output_var = true;

    public:
        parseFuzzConstructsVisitor(clang::Rewriter& _rw, clang::ASTContext& _ctx) :
            rw(_rw), ctx(_ctx) {};

        const clang::ast_type_traits::DynTypedNode
        getBaseParent(const clang::ast_type_traits::DynTypedNode dyn_node)
        {
            clang::ASTContext::DynTypedNodeList node_parents =
                this->ctx.getParents(dyn_node);
            assert(node_parents.size() == 1);
            if (node_parents[0].get<clang::CompoundStmt>() == main_child)
            {
                return dyn_node;
            }
            return this->getBaseParent(node_parents[0]);
        }

        bool
        VisitCallExpr(clang::CallExpr* ce)
        {
            if (clang::Decl* d = ce->getCalleeDecl())
            {
                if (clang::FunctionDecl* fd =
                        llvm::dyn_cast<clang::FunctionDecl>(d);
                    fd && !fd->getQualifiedNameAsString().compare("fuzz::start"))
                {
                    assert(!in_fuzz_template);
                    in_fuzz_template = true;
                }
                else if (clang::FunctionDecl* fd =
                        llvm::dyn_cast<clang::FunctionDecl>(d);
                    fd && !fd->getQualifiedNameAsString().compare("fuzz::end"))
                {
                    assert(in_fuzz_template);
                    in_fuzz_template = false;
                }
            }
            return true;
        }

        bool
        VisitVarDecl(clang::VarDecl* vd)
        {
            if (in_fuzz_template)
            {
                //std::cout << vd->getNameAsString() << std::endl;
                //common_template_var_decls.erase(vd);
                clang::ASTContext::DynTypedNodeList vd_parents =
                    this->ctx.getParents(*vd);
                assert(vd_parents.size() == 1);
                const clang::Stmt* base_parent =
                    this->getBaseParent(vd_parents[0]).get<clang::Stmt>();
                assert(base_parent);
                std::vector<stmtRedeclTemplateVars>::iterator srtv_it =
                    std::find_if(stmt_rewrite_map.begin(), stmt_rewrite_map.end(),
                    [&base_parent](stmtRedeclTemplateVars srtv)
                    {
                        return srtv.base_stmt == base_parent;
                    });
                assert(srtv_it != stmt_rewrite_map.end());
                (*srtv_it).decl_var_additions.push_back(
                    vd->getLocation().getLocWithOffset(vd->getName().size()));
                input_template_var_decls.insert(vd);
                declared_fuzz_vars.emplace(vd->getNameAsString(),
                    vd->getType().getAsString());
            }
            return true;
        }

        bool
        VisitDeclRefExpr(clang::DeclRefExpr* dre)
        {
            if (clang::VarDecl* vd = llvm::dyn_cast<clang::VarDecl>(dre->getDecl());
                vd && input_template_var_decls.count(vd))
            {
                clang::ASTContext::DynTypedNodeList dre_parents =
                    this->ctx.getParents(*dre);
                assert(dre_parents.size() == 1);
                const clang::Stmt* base_parent =
                    this->getBaseParent(dre_parents[0]).get<clang::Stmt>();
                assert(base_parent);
                std::vector<stmtRedeclTemplateVars>::iterator srtv_it =
                    std::find_if(stmt_rewrite_map.begin(), stmt_rewrite_map.end(),
                    [&base_parent](stmtRedeclTemplateVars srtv)
                    {
                        return srtv.base_stmt == base_parent;
                    });
                assert(srtv_it != stmt_rewrite_map.end());
                (*srtv_it).decl_var_additions.push_back(
                    dre->getLocation().getLocWithOffset(
                        dre->getDecl()->getName().size()));

                //stmt_rewrite_map.at(base_parent).push_back(
                    //dre->getLocation().getLocWithOffset(
                        //dre->getDecl()->getName().size()));
            }
            if (dre->hasQualifier())
            {
                clang::NamespaceDecl* nd = dre->getQualifier()->getAsNamespace();
                if (nd && !nd->getNameAsString().compare("fuzz"))
                {
                    if (!dre->getDecl()->getNameAsString().compare("output_var"))
                    {
                        clang::ASTContext::DynTypedNodeList dre_parents =
                            this->ctx.getParents(*dre);
                        assert(dre_parents.size() == 1);
                        const clang::Stmt* base_parent =
                            this->getBaseParent(dre_parents[0]).get<clang::Stmt>();
                        assert(base_parent);
                        std::vector<stmtRedeclTemplateVars>::iterator srtv_it =
                            std::find_if(stmt_rewrite_map.begin(), stmt_rewrite_map.end(),
                            [&base_parent](stmtRedeclTemplateVars srtv)
                            {
                                return srtv.base_stmt == base_parent;
                            });
                        assert(srtv_it != stmt_rewrite_map.end());

                        if (this->first_output_var)
                        {
                            //to_replace << dre->getType().getAsString() << " ";
                            (*srtv_it).output_var_type = dre->getType().getAsString();
                            (*srtv_it).output_var_decl = dre->getSourceRange();
                            this->first_output_var = false;

                            if (meta_input_var_type.empty())
                            {
                                meta_input_var_type = dre->getType().getAsString();
                            }
                            assert(!dre->getType().getAsString()
                                .compare(meta_input_var_type));
                        }
                        else
                        {
                            (*srtv_it).output_var_additions.push_back(dre->getSourceRange());
                        }
                        //for (size_t i = 0; i < meta_input_fuzz_count; ++i)
                        //{
                            //std::stringstream to_replace_tmp(to_replace.str());
                            //clang::Rewriter rw_tmp(rw.getSourceMgr(), rw.getLangOpts());
                            //rw_tmp.ReplaceText(sr, llvm::StringRef(to_replace_tmp.str()));
                            //input_template_copies.at(i).push_back(
                                //rw_tmp.getRewrittenText(sr));
                        //}
                        //rw.RemoveText(sr);
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
                        //declared_fuzz_vars.emplace(vd->getNameAsString(), vd->getType());
                        //addLibType(vd->getType().getAsString());
                        //addLibDeclaredObj(vd->getNameAsString(),
                            //vd->getType().getAsString());
                    }
                }
            }
            return true;
        }
};

class fuzzExpander
{
    private:
        static std::set<std::pair<std::string, std::string>>
        getDuplicateDeclVars(
            std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)> vars,
            size_t output_var_count)
        {
            std::set<std::pair<std::string, std::string>> duplicate_vars;
            for (fuzzVarDecl fvd : vars)
            {
                duplicate_vars.emplace(
                    fvd.name + "_" + std::to_string(output_var_count),
                    fvd.type);
            }
            std::for_each(common_template_var_decls.begin(),
                common_template_var_decls.end(),
                [&duplicate_vars](fuzzVarDecl fvd)
                {
                    duplicate_vars.emplace(fvd.name, fvd.type);
                });

            //for (const clang::VarDecl* vd : common_template_var_decls)
            //{
                //vd->dump();
                //std::cout << vd->getNameAsString() << std::endl;
                //duplicate_vars.emplace(vd->getNameAsString(),
                    //vd->getType().getAsString());
            //}
            return duplicate_vars;
        }

    public:
        static void
        expandLoggedNewVars(clang::Rewriter& rw, clang::ASTContext& ctx)
        {
            size_t curr_input_count = 0;
            fuzzer::clang::resetApiObjs(
                getDuplicateDeclVars(declared_fuzz_vars, curr_input_count));
            for (fuzzNewCall fnc : fuzz_new_vars)
            {
                if (fnc.start_fuzz_call)
                {
                    assert(false);
                    assert(!fnc.base_stmt && !fnc.fuzz_ref &&
                        !fnc.reset_fuzz_var_decl);
                    rw.RemoveText(fnc.start_fuzz_call->getSourceRange());
                    continue;
                }
                if (fnc.reset_fuzz_var_decl)
                {
                    assert(!fnc.base_stmt && !fnc.fuzz_ref &&
                        !fnc.start_fuzz_call && fnc.reset_fuzz_call);
                    fuzzer::clang::resetApiObjs(
                        getDuplicateDeclVars(
                            declared_fuzz_vars, ++curr_input_count));
                    rw.RemoveText(fnc.reset_fuzz_call->getSourceRange());
                    continue;
                }
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
                    fuzzer::clang::generateObjectInstructions(
                        fuzz_ref->template_arguments()[0].getArgument()
                        .getAsType().getAsString(), indent);
                rw.InsertText(base_stmt->getBeginLoc(), fuzzer_output.first + indent.str());
                rw.ReplaceText(clang::SourceRange(fuzz_call->getBeginLoc(),
                    fuzz_call->getEndLoc()), fuzzer_output.second);
            }
        }

        static void
        expandMetaTests(clang::Rewriter& rw, clang::ASTContext& ctx)
        {
            assert(!meta_input_var_type.empty());
            assert(!set_meta_tests_path.empty());
            std::vector<std::string> input_var_names;
            for (size_t i = 0; i < meta_input_fuzz_count; ++i)
            {
                input_var_names.push_back("output_var_" + std::to_string(i));
            }
            for (const clang::CallExpr* meta_call : meta_test_calls)
            {
                const std::string indent =
                    clang::Lexer::getIndentationForLine(meta_call->getBeginLoc(),
                        rw.getSourceMgr()).str();
                rw.ReplaceText(meta_call->getSourceRange(),
                    fuzzer::clang::generateMetaTestInstructions(input_var_names,
                        meta_input_var_type, indent, set_meta_tests_path,
                        meta_test_rel_count));
            }
        }
};

class newVariableFuzzerParser : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::CallExpr* ce =
                    Result.Nodes.getNodeAs<clang::CallExpr>("metaTestCall"))
            {
                meta_test_calls.push_back(ce);
            }
            //else if (const clang::VarDecl* vd =
                    //Result.Nodes.getNodeAs<clang::VarDecl>("mainVarDecl"))
            //{
                //common_template_var_decls.insert(vd);
            //}
            else
            {
                fuzzNewCall fnc;
                if (const clang::Stmt* s =
                        Result.Nodes.getNodeAs<clang::Stmt>("baseStmt"))
                {
                    //s->dump();
                    fnc.base_stmt = s;
                }
                if (const clang::DeclRefExpr* dre =
                        Result.Nodes.getNodeAs<clang::DeclRefExpr>("fuzzRef"))
                {
                    //dre->dump();
                    fnc.fuzz_ref = dre;
                }
                if (const clang::CallExpr* ce =
                        Result.Nodes.getNodeAs<clang::CallExpr>("outputVarEnd"))
                {
                    fnc.reset_fuzz_call = ce;
                    fnc.reset_fuzz_var_decl = true;
                }

                //if (const clang::CallExpr* ce =
                        //Result.Nodes.getNodeAs<clang::CallExpr>("outputVarStart"))
                //{
                    //fnc.start_fuzz_call = ce;
                //}
                fuzz_new_vars.push_back(fnc);
            }
        }
};

class newVariableStatementRemover : public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::Rewriter& rw;

    public:
        newVariableStatementRemover(clang::Rewriter& _rw) : rw(_rw) {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::CallExpr* ce =
                    Result.Nodes.getNodeAs<clang::CallExpr>("outputVarStart"))
            {
                rw.RemoveText(ce->getSourceRange());
            }
            else if (const clang::NullStmt* ns =
                    Result.Nodes.getNodeAs<clang::NullStmt>("empty"))
            {
                rw.RemoveText(ns->getSourceRange());
            }
        }
};

class newVariableFuzzerMatcher : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        newVariableFuzzerParser parser;
        newVariableStatementRemover remover;

    public:
        newVariableFuzzerMatcher(clang::Rewriter& _rw) :
            remover(newVariableStatementRemover(_rw))
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
                clang::ast_matchers::declRefExpr(
                clang::ast_matchers::to(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::fuzz_new"))))
                    .bind("fuzzRef"))) )
                    .bind("baseStmt"), &parser);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::end"))))
                    .bind("outputVarEnd"), &parser);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::meta_test"))))
                    .bind("metaTestCall"), &parser);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::start"))))
                    .bind("outputVarStart"), &remover);

            //matcher.addMatcher(
                //clang::ast_matchers::nullStmt()
                    //.bind("empty"), &remover);
                    //

            //matcher.addMatcher(
                //clang::ast_matchers::varDecl(
                //clang::ast_matchers::allOf(
                    //clang::ast_matchers::hasAncestor(
                    //clang::ast_matchers::functionDecl(
                    //clang::ast_matchers::isMain()))
                    //,
                    //clang::ast_matchers::unless(
                    //clang::ast_matchers::parmVarDecl())
                //))
                    //.bind("mainVarDecl"), &parser);
        }

        void matchAST(clang::ASTContext& ctx)
        {
            matcher.matchAST(ctx);
        }
};

class parseFuzzConstructs : public clang::ASTConsumer
{
    private:
        //gatherDeclaredObjsVisitor gatherDeclObjsVis;
        newVariableFuzzerMatcher newVarFuzzerMtch;
        clang::Rewriter& rw;

    public:
        parseFuzzConstructs(clang::Rewriter& _rw) : rw(_rw),
            newVarFuzzerMtch(newVariableFuzzerMatcher(_rw)) {};

        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            //gatherDeclObjsVis.TraverseDecl(ctx.getTranslationUnitDecl());
            newVarFuzzerMtch.matchAST(ctx);
            fuzzExpander::expandLoggedNewVars(rw, ctx);
            fuzzExpander::expandMetaTests(rw, ctx);
        }
};

class templateLocLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::SourceManager& SM;

    public:
        templateLocLogger(clang::SourceManager& _SM) : SM(_SM) {};

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
            else if (const clang::CompoundStmt* cs =
                    Result.Nodes.getNodeAs<clang::CompoundStmt>("mainChild"))
            {
                main_child = cs;
            }
            else if (const clang::VarDecl* vd =
                    Result.Nodes.getNodeAs<clang::VarDecl>("mainVarDecl");
                    vd && !inFuzzTemplate(vd, SM))
            {
                //vd->dump();
                common_template_var_decls.emplace(vd->getNameAsString(),
                    vd->getType().getAsString());
            }
        }
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
        templateDuplicator(clang::Rewriter& _rw) : rw(_rw),
            logger(templateLocLogger(_rw.getSourceMgr()))
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

            matcher.addMatcher(
                clang::ast_matchers::compoundStmt(
                clang::ast_matchers::hasParent(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::isMain())))
                    .bind("mainChild"), &logger);

            matcher.addMatcher(
                clang::ast_matchers::varDecl(
                clang::ast_matchers::allOf(
                    clang::ast_matchers::hasAncestor(
                    clang::ast_matchers::functionDecl(
                    clang::ast_matchers::isMain()))
                    ,
                    clang::ast_matchers::unless(
                    clang::ast_matchers::parmVarDecl())
                ))
                    .bind("mainVarDecl"), &logger);

            for (size_t i = 0; i < meta_input_fuzz_count; ++i)
            {
                std::vector<std::string> input_template_strs;
                input_template_strs.push_back(
                    "/* Template initialisation for output var " +
                    std::to_string(i) + " */\n");
                std::pair<size_t, std::vector<std::string>> template_str_pair(i, input_template_strs);
                input_template_copies.insert(template_str_pair);
            }
        };

        // TODO rather than remove, duplicate
        void
        HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);

            bool in_fuzz_template = false;
            for (clang::Stmt::const_child_iterator it = main_child->child_begin();
                it != main_child->child_end(); ++it)
            {
                if (in_fuzz_template)
                {
                    stmt_rewrite_map.emplace_back(*it);
                }
                if (const clang::CallExpr* call_child =
                    llvm::dyn_cast<clang::CallExpr>(*it))
                {
                    if (const clang::FunctionDecl* fn_child =
                        llvm::dyn_cast<clang::CallExpr>(*it)->getDirectCallee())
                    {
                        if (!fn_child->getQualifiedNameAsString()
                                .compare("fuzz::start"))
                        {
                            assert(!in_fuzz_template);
                            in_fuzz_template = true;
                        }
                        else if (!fn_child->getQualifiedNameAsString()
                                .compare("fuzz::end"))
                        {
                            assert(in_fuzz_template);
                            in_fuzz_template = false;
                        }
                    }
                }
            }

            parseFuzzConstructsVisitor parseConstructsVis(rw, ctx);
            parseConstructsVis.TraverseDecl(ctx.getTranslationUnitDecl());

            for (stmtRedeclTemplateVars stmt_redecl :
                        stmt_rewrite_map)
            {
                //stmt_redecl.base_stmt->dump();
                const llvm::StringRef indent =
                    clang::Lexer::getIndentationForLine(
                        stmt_redecl.base_stmt->getBeginLoc(),
                        rw.getSourceMgr());
                for (size_t i = 0; i < meta_input_fuzz_count; ++i)
                {
                    clang::Rewriter rw_tmp(rw.getSourceMgr(), rw.getLangOpts());
                    for (clang::SourceLocation sl :
                            stmt_redecl.decl_var_additions)
                    {
                        rw_tmp.InsertText(sl, "_" + std::to_string(i));
                    }
                    if (stmt_redecl.output_var_decl.isValid())
                    {
                        std::stringstream output_var_decl_rw;
                        output_var_decl_rw << stmt_redecl.output_var_type;
                        output_var_decl_rw << " output_var_" << i;
                        rw_tmp.ReplaceText(stmt_redecl.output_var_decl,
                            output_var_decl_rw.str());
                    }
                    for (clang::SourceRange sr :
                            stmt_redecl.output_var_additions)
                    {
                        rw_tmp.ReplaceText(sr, "output_var_" + std::to_string(i));
                    }
                    input_template_copies.at(i).
                        push_back((indent + rw_tmp.getRewrittenText(
                            stmt_redecl.base_stmt->getSourceRange())).str());
                }
                rw.RemoveText(stmt_redecl.base_stmt->getSourceRange());
            }

            for (std::pair<size_t, std::vector<std::string>> fuzzed_template_copies :
                    input_template_copies)
            {
                std::string template_copy_str = std::accumulate(
                    fuzzed_template_copies.second.begin(),
                    fuzzed_template_copies.second.end(), std::string(),
                    [](std::string acc, std::string next_instr)
                    {
                        return acc + next_instr + ";\n";

                    });
                rw.InsertText(fuzz_template_bounds.second->getBeginLoc(),
                    template_copy_str);
            }

            //rw.RemoveText(fuzz_template_bounds.first->getSourceRange());

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
            //llvm::raw_string_ostream rso(rewrite_data);
            //rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rso);

            std::error_code ec;
            int fd;
            llvm::sys::fs::createTemporaryFile("", ".cpp", fd,
                rewritten_input_file);
            llvm::raw_fd_ostream rif_rfo(fd, true);
            rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
        }

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

        bool
        BeginSourceFileAction(clang::CompilerInstance& ci) override
        {
            std::cout << "[parseFuzzConstructsAction] Parsing input file ";
            std::cout << ci.getSourceManager().getFileEntryForID(
                ci.getSourceManager().getMainFileID())->getName().str()
                    << std::endl;
            return true;
        }

        void
        EndSourceFileAction() override
        {
            //llvm::raw_string_ostream rso(rewrite_data);
            //rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rso);

            std::error_code ec;
            int fd;
            llvm::sys::fs::createTemporaryFile("", ".cpp", fd,
                rewritten_input_file);
            llvm::raw_fd_ostream rif_rfo(fd, true);
            rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
            //llvm::sys::fs::remove(rewritten_input_file);

            //assert(!output_file.empty());
            //std::error_code ec;
            //llvm::raw_fd_ostream of_rfo(output_file, ec);
            //rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
                //.write(of_rfo);
            //of_rfo.close();
            //rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
                //.write(llvm::outs());
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
