 /******************************************************************************
 *  Copyright 2021 Andrei Lascu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "parseFuzzSpec.hpp"

//std::set<fuzzVarDecl, decltype(&fuzzVarDecl::compare)> template_fuzz_vars(&fuzzVarDecl::compare);
//std::map<fuzzVarDecl, std::set<const clang::VarDecl*>, decltype(&fuzzVarDecl::compare)>
    //template_fuzz_vars(&fuzzVarDecl::compare);

static std::map<std::string, clang::APValue*> config_inputs;
static std::pair<const clang::CallExpr*, const clang::CallExpr*>
    fuzz_template_bounds(nullptr, nullptr);
static std::set<clang::VarDecl*> input_template_var_decls;
static std::map<size_t, std::vector<std::string>>
    input_template_copies;
static const clang::CompoundStmt* main_child;

static std::vector<fuzzNewCall> fuzz_new_vars;
std::vector<std::pair<const clang::Stmt*, fuzzVarDecl>> spec_vars;
static std::vector<std::tuple<
    const clang::CallExpr*, const clang::Stmt*, const clang::FunctionDecl*>>
        mr_fuzz_calls;
static std::vector<stmtRedeclTemplateVars> stmt_rewrite_map;
static std::vector<std::pair<const clang::CallExpr*, size_t>> miv_get_calls;

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

void
fuzzConfigRecorder::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::VarDecl* VD =
            Result.Nodes.getNodeAs<clang::VarDecl>("inputDecl"))
    {
        config_inputs.insert(std::make_pair(VD->getNameAsString(), VD->evaluateValue()));
    }
}

fuzzConfigParser::fuzzConfigParser()
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
fuzzConfigParser::HandleTranslationUnit(clang::ASTContext& ctx)
{
    matcher.matchAST(ctx);
}

std::string
templateVariableDuplicatorVisitor::getText()
{
    std::string ss_str;
    llvm::raw_string_ostream ss(ss_str);
    rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(ss);
    return ss.str();
}

bool
templateVariableDuplicatorVisitor::VisitVarDecl(clang::VarDecl* vd)
{
    rw.InsertText(vd->getLocation().getLocWithOffset(vd->getName().size()),
        globals::suffix_delim + std::to_string(id));
    return true;
}

bool
templateVariableDuplicatorVisitor::VisitDeclRefExpr(clang::DeclRefExpr* dre)
{
    if (llvm::dyn_cast<clang::VarDecl>(dre->getDecl()))
    {
        rw.InsertText(dre->getEndLoc(), globals::suffix_delim + std::to_string(id));
    }
    return true;
}

//==============================================================================

const clang::ast_type_traits::DynTypedNode
parseFuzzConstructsVisitor::getBaseParent(
    const clang::ast_type_traits::DynTypedNode dyn_node)
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
parseFuzzConstructsVisitor::VisitCallExpr(clang::CallExpr* ce)
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
parseFuzzConstructsVisitor::VisitVarDecl(clang::VarDecl* vd)
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
    }
    return true;
}

bool
parseFuzzConstructsVisitor::VisitDeclRefExpr(clang::DeclRefExpr* dre)
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
            if (!dre->getDecl()->getNameAsString().compare(globals::meta_input_var_prefix))
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
                    this->first_output_var = false;

                    if (this->meta_input_var_type == nullptr)
                    {
                        this->meta_input_var_type = dre->getType().getTypePtr();
                    }
                    assert(this->meta_input_var_type == dre->getType().getTypePtr());

                    (*srtv_it).output_var_type = meta_input_var_type->getCanonicalTypeInternal().getAsString();
                    // TODO consider a better way to prune unwanted keywords
                    size_t class_pos = (*srtv_it).output_var_type.find("class");
                    if (class_pos != std::string::npos)
                    {
                        (*srtv_it).output_var_type =
                            (*srtv_it).output_var_type.replace(class_pos, sizeof("class"), "");
                    }
                    (*srtv_it).output_var_decl = dre->getSourceRange();
                }
                else
                {
                    (*srtv_it).output_var_additions.push_back(dre->getSourceRange());
                }
            }
        }
    }
    return true;
}

//==============================================================================

std::set<std::pair<std::string, std::string>>
fuzzExpander::getDuplicateDeclVars(
    std::vector<std::pair<const clang::Stmt*, fuzzVarDecl>> vars,
    size_t output_var_count, bool filter_template)
{
    std::set<std::pair<std::string, std::string>> duplicate_vars;
    std::for_each(vars.begin(), vars.end(),
        [&duplicate_vars, &output_var_count, &filter_template]
            (std::pair<const clang::Stmt*, fuzzVarDecl> vars_pair)
        {
            if (fuzzVarDecl s_var = vars_pair.second; s_var.vd != nullptr)
            {
                if (size_t suf_loc = s_var.getName().rfind(globals::suffix_delim);
                        filter_template && s_var.in_template &&
                        suf_loc != std::string::npos &&
                        std::stoi(s_var.getName().substr(suf_loc + 1)) != output_var_count)
                {
                    return;
                }
                //std::string s_var_name = s_var.in_template
                    //? s_var.getName() + globals::suffix_delim + std::to_string(output_var_count)
                    //: s_var.getName();
                duplicate_vars.emplace(s_var.getName(), s_var.getTypeName());
            }
        });
    //for (fuzzVarDecl fvd : vars)
    //{
        //duplicate_vars.emplace(
            //fvd.name + "_" + std::to_string(output_var_count),
            //fvd.type);
    //}

    return duplicate_vars;
}

void
fuzzExpander::expandLoggedNewVars(clang::Rewriter& rw, clang::ASTContext& ctx)
{
    size_t curr_input_count = -1;
    bool in_template = false;
    std::vector<std::pair<const clang::Stmt*, fuzzVarDecl>>::iterator tfv =
        spec_vars.begin();
    for (fuzzNewCall fnc : fuzz_new_vars)
    {
        if (fnc.start_fuzz_call)
        {
            assert(!fnc.base_stmt && !fnc.fuzz_ref &&
                !fnc.end_fuzz_call);
            curr_input_count += 1;
            in_template = true;
            rw.RemoveText(fnc.start_fuzz_call->getSourceRange());
            continue;
        }
        if (fnc.end_fuzz_call /*&& tfv != template_fuzz_vars.end()*/)
        {
            assert(!fnc.base_stmt && !fnc.fuzz_ref && !fnc.start_fuzz_call);
            // TODO could know where to set the iterator to instead of restarting
            // the search
            tfv = spec_vars.begin();
            in_template = false;
            if (curr_input_count  == globals::meta_input_fuzz_count)
            {
                curr_input_count = -1;
            }

            rw.RemoveText(fnc.end_fuzz_call->getSourceRange());
            continue;
        }
        else
        {
            assert(fnc.base_stmt);
            while(fnc.base_stmt != (*tfv).first)
            {
                tfv++;
                // Missing base_stmt in spec_vars
                assert(tfv != spec_vars.end());
            }


            //if (fnc.template_var_vd)
            //{
                //while(fnc.template_var_vd->getNameAsString().find(
                    //(*tfv).second.name) == std::string::npos)
                //{
                    //tfv++;
                    //CHECK_CONDITION(tfv != spec_vars.end(),
                        //"Could not find VarDecl for var " +
                        //fnc.template_var_vd->getNameAsString());
                //}
            //}
            // TODO check if rhs of VarDecl contains references to fuzz_var or fuzz_new
            fuzzer::clang::resetApiObjs(getDuplicateDeclVars(
                    std::vector<std::pair<const clang::Stmt*, fuzzVarDecl>>(
                    spec_vars.begin(), tfv), curr_input_count, in_template));
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

void fuzzExpander::expandLoggedNewMRVars(clang::Rewriter& rw, clang::ASTContext& ctx)
{
    std::set<std::pair<std::string, std::string>> mr_vars;

    // Add MR parameter vars
    fuzzer::clang::resetApiObjs(mr_vars);
    for (std::tuple<const clang::CallExpr*, const clang::Stmt*, const clang::FunctionDecl*>
            mrfc : mr_fuzz_calls)
    {
        const clang::CallExpr* ce = std::get<0>(mrfc);
        const clang::Stmt* base_stmt = std::get<1>(mrfc);
        const clang::FunctionDecl* mr_decl = std::get<2>(mrfc);
        mr_vars.clear();

        //ce->dump();
        //base_stmt->dump();
        //mr_decl->dump();

        clang::FunctionDecl::param_const_iterator mr_param_it =
            mr_decl->param_begin();
        while (mr_param_it != mr_decl->param_end())
        {
            mr_vars.insert(std::make_pair((*mr_param_it)->getNameAsString(),
                (*mr_param_it)->getType().getAsString())) ;
            ++mr_param_it;
        }
        fuzzer::clang::resetApiObjs(mr_vars);

        const llvm::StringRef indent =
            clang::Lexer::getIndentationForLine(base_stmt->getBeginLoc(),
                rw.getSourceMgr());
        const clang::DeclRefExpr* ce_dre =
            llvm::dyn_cast<clang::DeclRefExpr>(
                llvm::dyn_cast<clang::ImplicitCastExpr>(ce->getCallee())->getSubExpr());
        assert(ce_dre);
        std::pair<std::string, std::string> fuzzer_output =
            fuzzer::clang::generateObjectInstructions(
                ce_dre->template_arguments()[0].getArgument().getAsType().getAsString(), "\t");
        rw.InsertText(base_stmt->getBeginLoc(), fuzzer_output.first + indent.str());
        rw.ReplaceText(clang::SourceRange(ce->getBeginLoc(),
            ce->getEndLoc()), fuzzer_output.second);
    }

}

//==============================================================================

void
templateVarLogger::run(
    const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    fuzzNewCall fnc;
    if (const clang::Stmt* s =
            Result.Nodes.getNodeAs<clang::Stmt>("baseStmt"))
    {
        if (const clang::DeclRefExpr* dre = Result.Nodes.getNodeAs<clang::DeclRefExpr>("mainFuzzCallRef"))
        {
            fnc.base_stmt = s;
            fnc.template_var_vd = Result.Nodes.getNodeAs<clang::VarDecl>("mainVarDecl");
            fnc.fuzz_ref = dre;
            assert(fnc.fuzz_ref);
            spec_vars.emplace_back(s,
                fuzzVarDecl(fnc.template_var_vd, s, this->in_template));
        }
        else
        {
            spec_vars.emplace_back(s,
                fuzzVarDecl(
                Result.Nodes.getNodeAs<clang::VarDecl>("mainVarDecl"),
                s, this->in_template));
            return;
        }
    }
    else if (const clang::CallExpr* ce = Result.Nodes.getNodeAs<clang::CallExpr>("outputVarEnd"))
    {
        this->in_template = false;
        fnc.end_fuzz_call = ce;
    }
    else if (const clang::CallExpr* ce = Result.Nodes.getNodeAs<clang::CallExpr>("outputVarStart"))
    {
        this->in_template = true;
        fnc.start_fuzz_call = ce;
    }
    else
    {
        assert(false);
    }
    //else if (const clang::CallExpr* ce = Result.Nodes.getNodeAs(clang:CallExpr>("outputVarStart")))
    //{
        //fnc.start_fuzz_call = ce;
    //}
    fuzz_new_vars.push_back(fnc);
}

void
mrNewVariableFuzzerLogger::run(
    const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::CallExpr* ce = Result.Nodes.getNodeAs<clang::CallExpr>("fuzzRef");
    assert(ce);
    const clang::Stmt* base_stmt = Result.Nodes.getNodeAs<clang::Stmt>("baseStmt");
    assert(base_stmt);
    const clang::FunctionDecl* mr_decl = Result.Nodes.getNodeAs<clang::FunctionDecl>("mrDecl");
    mr_fuzz_calls.push_back(std::make_tuple(ce, base_stmt, mr_decl));
}

//==============================================================================

void
newVariableStatementRemover::run(
    const clang::ast_matchers::MatchFinder::MatchResult& Result)
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

//==============================================================================

newVariableFuzzerMatcher::newVariableFuzzerMatcher(clang::Rewriter& _rw) :
    remover(newVariableStatementRemover(_rw))
{
            matcher.addMatcher(
                clang::ast_matchers::stmt(
                clang::ast_matchers::allOf(
                    clang::ast_matchers::hasParent(
                    clang::ast_matchers::compoundStmt(
                    clang::ast_matchers::hasParent(
                    clang::ast_matchers::functionDecl(
                    clang::ast_matchers::isMain()))))
                    ,
                    clang::ast_matchers::unless(
                    clang::ast_matchers::nullStmt())
                    ,
                    clang::ast_matchers::anyOf(
                    clang::ast_matchers::hasDescendant(
                    clang::ast_matchers::varDecl()
                        .bind("mainVarDecl")),
                    clang::ast_matchers::anything())
                    ,
                    clang::ast_matchers::anyOf(
                    clang::ast_matchers::hasDescendant(
                    clang::ast_matchers::declRefExpr(
                    clang::ast_matchers::to(
                    clang::ast_matchers::functionDecl(
                    clang::ast_matchers::hasName(
                    "fuzz::fuzz_new"))))
                        .bind("mainFuzzCallRef"))
                    ,
                    clang::ast_matchers::anything())))
                        .bind("baseStmt"), &template_var_logger);

            //matcher.addMatcher(
                //clang::ast_matchers::stmt(
                //clang::ast_matchers::allOf(
                    //clang::ast_matchers::hasParent(
                    //clang::ast_matchers::compoundStmt(
                    //clang::ast_matchers::hasParent(
                    //clang::ast_matchers::functionDecl(
                    //clang::ast_matchers::isMain()))))
                    //,
                    //clang::ast_matchers::anyOf(
                        //clang::ast_matchers::hasDescendant(
                        //clang::ast_matchers::varDecl()
                        //.bind("mainVarDecl"))
                        //,
                        //clang::ast_matchers::anything())
                    //)
                //)
                //.bind("mainVarDeclStmt"), &template_var_logger);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::allOf(

                // Only check in metamorphic relations
                clang::ast_matchers::unless(
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::isMain()))),

                // ... while recording full call statement ...
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::stmt(
                clang::ast_matchers::hasParent(
                clang::ast_matchers::compoundStmt(
                clang::ast_matchers::hasParent(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasParent(
                clang::ast_matchers::translationUnitDecl()))
                    .bind("mrDecl")))))
                    .bind("baseStmt")),

                // ... of fuzz_new fuzzer calls.
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::fuzz_new")))))
                    .bind("fuzzRef"), &mr_fuzzer_logger);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::end"))))
                    .bind("outputVarEnd"), &template_var_logger);

            matcher.addMatcher(
                clang::ast_matchers::callExpr(
                clang::ast_matchers::callee(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasName(
                "fuzz::start"))))
                    .bind("outputVarStart"), &template_var_logger);

            //matcher.addMatcher(
                //clang::ast_matchers::callExpr(
                //clang::ast_matchers::callee(
                //clang::ast_matchers::functionDecl(
                //clang::ast_matchers::hasName(
                //"fuzz::start"))))
                    //.bind("outputVarStart"), &remover);
}

//==============================================================================

void
templateLocLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::CallExpr* start_ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("startTemplate"))
    {
        fuzz_template_bounds.first = start_ce;
        return;
    }
    else if (const clang::CallExpr* end_ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("endTemplate"))
    {
        fuzz_template_bounds.second = end_ce;
        return;
    }
    else if (const clang::CompoundStmt* cs =
            Result.Nodes.getNodeAs<clang::CompoundStmt>("mainChild"))
    {
        main_child = cs;
        return;
    }
    else if (const clang::CallExpr* ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("mivGetCE"))
    {
        assert(ce->getNumArgs() == 1);
        miv_get_calls.emplace_back(ce,
            Result.Nodes.getNodeAs<clang::IntegerLiteral>("mivID")->getValue().getSExtValue());
        return;
    }
    assert(false);
}

//==============================================================================

templateDuplicator::templateDuplicator(clang::Rewriter& _rw) :
    rw(_rw), logger(templateLocLogger(_rw.getSourceMgr()))
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
        clang::ast_matchers::callExpr(
        clang::ast_matchers::allOf(
            clang::ast_matchers::callee(
            clang::ast_matchers::functionDecl(
            clang::ast_matchers::hasName(
                globals::meta_input_var_get_prefix))),

            clang::ast_matchers::hasArgument(
                0, clang::ast_matchers::integerLiteral()
                .bind("mivID"))))
                .bind("mivGetCE"), &logger);

    for (size_t i = 0; i < globals::meta_input_fuzz_count; ++i)
    {
        std::vector<std::string> input_template_strs;
        input_template_strs.push_back(
            "/* Template initialisation for output var " +
            std::to_string(i) + " */\n");
        std::pair<size_t, std::vector<std::string>> template_str_pair(i, input_template_strs);
        input_template_copies.insert(template_str_pair);
    }
};

void
templateDuplicator::HandleTranslationUnit(clang::ASTContext& ctx)
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
                    // Add `fuzz::start` CallExpr for later processing
                    stmt_rewrite_map.emplace_back(*it);
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
        for (size_t i = 0; i < globals::meta_input_fuzz_count; ++i)
        {
            clang::Rewriter rw_tmp(rw.getSourceMgr(), rw.getLangOpts());
            for (clang::SourceLocation sl :
                    stmt_redecl.decl_var_additions)
            {
                rw_tmp.InsertText(sl, globals::suffix_delim + std::to_string(i));
            }
            if (stmt_redecl.output_var_decl.isValid())
            {
                std::stringstream output_var_decl_rw;
                output_var_decl_rw << stmt_redecl.output_var_type;
                output_var_decl_rw << " " << fuzz_helpers::getMetaInputVarName(i);
                rw_tmp.ReplaceText(stmt_redecl.output_var_decl,
                    output_var_decl_rw.str());
            }
            for (clang::SourceRange sr :
                    stmt_redecl.output_var_additions)
            {
                rw_tmp.ReplaceText(sr, fuzz_helpers::getMetaInputVarName(i));
            }
            input_template_copies.at(i).
                push_back((indent + rw_tmp.getRewrittenText(
                    stmt_redecl.base_stmt->getSourceRange())).str());
        }
        rw.RemoveText(stmt_redecl.base_stmt->getSourceRange());
    }

    if (fuzz_template_bounds.first)
    {
        assert(fuzz_template_bounds.second);

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
    }

    for (std::pair<const clang::CallExpr*, size_t> miv_get_pair : miv_get_calls)
    {
        rw.ReplaceText(miv_get_pair.first->getSourceRange(),
            fuzz_helpers::getMetaInputVarName(miv_get_pair.second));
    }

    //rw.RemoveText(fuzz_template_bounds.first->getSourceRange());
};

bool
templateDuplicatorAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "templateDuplicatorAction");
    return true;
};

void
templateDuplicatorAction::EndSourceFileAction()
{
    std::error_code ec;
    int fd;
    llvm::sys::fs::createTemporaryFile("mtFuzz", ".cpp", fd,
        globals::rewritten_input_file);
    llvm::raw_fd_ostream rif_rfo(fd, true);
    rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
    //
    //rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
        //.write(llvm::outs());
}

std::unique_ptr<clang::ASTConsumer>
templateDuplicatorAction::CreateASTConsumer(clang::CompilerInstance& ci,
    llvm::StringRef file)
{
    rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<templateDuplicator>(rw);
}

void
parseFuzzConstructs::HandleTranslationUnit(clang::ASTContext& ctx)
{
    newVarFuzzerMtch.matchAST(ctx);
    fuzzExpander::expandLoggedNewVars(rw, ctx);
    fuzzExpander::expandLoggedNewMRVars(rw, ctx);
}

bool
parseFuzzConstructsAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "parseFuzzConstructsAction");
    return true;
}

void
parseFuzzConstructsAction::EndSourceFileAction()
{
    std::error_code ec;
    int fd;
    llvm::sys::fs::createTemporaryFile("mtFuzz", "cpp", fd,
        globals::rewritten_input_file);
    llvm::raw_fd_ostream rif_rfo(fd, true);
    rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
    //llvm::sys::fs::remove(rewritten_input_file);

}

std::unique_ptr<clang::ASTConsumer>
parseFuzzConstructsAction::CreateASTConsumer(clang::CompilerInstance& ci,
    llvm::StringRef file)
{
    rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<parseFuzzConstructs>(rw);
}
