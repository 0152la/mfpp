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

#include "generateMetaTests.hpp"

static std::map<std::string, std::vector<const clang::VarDecl*>>
    main_var_decls_typed;
static std::vector<const clang::CallExpr*> meta_test_calls;
static std::map<size_t, std::vector<std::string>> mr_input_vars;
static const clang::FunctionDecl* test_main_fd;
std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
std::vector<mrInfo> meta_check_decls;

const clang::Type* meta_input_var_type = nullptr;
std::string meta_input_var_type_name = "";

std::map<std::string, REL_TYPE> mr_type_map {
    { "generators" , GENERATOR },
    { "relations"  , RELATION },
    { "checks"     , CHECK },
};

std::map<REL_TYPE, std::string> mr_type_string_map {
    { GENERATOR, "generators" },
    { RELATION , "relations" },
    { CHECK    , "checks" },
};

bool
MRTraverser::TraverseDecl(clang::Decl* D)
{
    clang::RecursiveASTVisitor<MRTraverser>::TraverseDecl(D);
    return true;
}

bool
MRTraverser::VisitCallExpr(clang::CallExpr* ce)
{
    // TODO fix these strings to be variables
    if (const clang::FunctionDecl* fd = ce->getDirectCallee())
    {
        std::string fd_name = fd->getNameAsString();
        if (!fd_name.compare("placeholder"))
        {
            this->mri.recursive_calls.emplace(ce);
            this->mri.is_base_func = false;
        }
        else if (!fd_name.compare("fuzz_rand") ||
                    !fd_name.compare("fuzz_new"))
        {
            this->mri.has_fuzz_call = true;
        }
    }
    clang::RecursiveASTVisitor<MRTraverser>::VisitCallExpr(ce);
    return true;
}

bool
MRTraverser::VisitVarDecl(clang::VarDecl* vd)
{
    this->mri.body_vd.emplace(vd);
    clang::RecursiveASTVisitor<MRTraverser>::VisitVarDecl(vd);
    return true;
}

bool
MRTraverser::VisitDeclRefExpr(clang::DeclRefExpr* dre)
{
    this->mri.body_dre.emplace(dre);
    clang::RecursiveASTVisitor<MRTraverser>::VisitDeclRefExpr(dre);
    return true;
}

bool
MRTraverser::TraverseStmt(clang::Stmt* S)
{
    clang::RecursiveASTVisitor<MRTraverser>::TraverseStmt(S);
    return true;
}

bool
MRTraverser::TraverseType(clang::QualType T)
{
    clang::RecursiveASTVisitor<MRTraverser>::TraverseType(T);
    return true;
}

std::string
metaGenerator::generateMetaTests(std::vector<std::string> input_var_names)
{
    std::vector<std::string> meta_family_chain;
    std::set<std::string> meta_families;

    // Populate metamorphic families
    std::for_each(std::begin(meta_rel_decls), std::end(meta_rel_decls),
        [&meta_families](std::pair<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> mr_decl)
        {
            if (mr_decl.first.first == REL_TYPE::RELATION)
            {
                meta_families.insert(mr_decl.first.second);
            }
        });
    assert(!meta_families.empty());
    //std::cout << " === META FAMILIES" << std::endl;
    //for (std::string f : meta_families)
    //{
        //std::cout << f << std::endl;
    //}

    // Generate meta family chain
    for (size_t i = 0; i < globals::meta_test_rel_count; ++i)
    {
        std::set<std::string>::const_iterator it = meta_families.cbegin();
        std::advance(it, fuzzer::clang::generateRand(0, meta_families.size() - 1));
        meta_family_chain.push_back(*it);
        mr_input_vars.emplace(i, std::vector<std::string>());
    }
    std::stringstream meta_tests;
    meta_tests << '\n';

    // Generate single metamorphic test
    for (int i = 0; i < globals::meta_test_count; ++i)
    {
        std::string meta_test = this->generateSingleMetaTest(input_var_names,
            meta_family_chain, i);
        meta_tests << "// Meta test " << i << std::endl;
        meta_tests << meta_test << std::endl;
    }
    //std::cout << " === META TESTS" << std::endl;
    //std::cout << meta_tests.str();
    return meta_tests.str();
}

std::string
metaGenerator::generateSingleMetaTest(std::vector<std::string> input_var_names,
    const std::vector<std::string>& meta_family_chain, size_t test_count)
{
    std::stringstream mt_body;
    std::string curr_mr_var_name = fuzz_helpers::getMetaVarName(test_count);
    mrGenInfo mgi(curr_mr_var_name, input_var_names, test_count);
    for (std::string meta_family : meta_family_chain)
    {
        std::vector<mrInfo> meta_rel_choices =
            meta_rel_decls.at(std::make_pair(REL_TYPE::RELATION, meta_family));
        mrInfo chosen_mr = meta_rel_choices.at(
            fuzzer::clang::generateRand(0, meta_rel_choices.size() - 1));
        mgi.setMR(&chosen_mr);

        std::pair<std::string, std::string> rw_meta_rel =
            concretizeMetaRelation(mgi);
        mgi.recursive_idx += 1;
        mgi.family_idx += 1;

        mt_body << rw_meta_rel.first;
        this->rw.InsertText(test_main_fd->getBeginLoc(), this->indent + rw_meta_rel.second);
        //std::cout << "ONE META TESTS" << std::endl;
        //std::cout << rw_meta_rel.first;
        //std::cout << "ONE RECURSIVE META FUNCTIONS" << std::endl;
        //std::cout << rw_meta_rel.second;
    }
    if (test_count == 0 && !globals::trivial_check)
    {
        return mt_body.str();
    }
    for (mrInfo meta_check : meta_check_decls)
    {

        mgi.setMR(&meta_check);
        mgi.family_idx = -1;
        mgi.input_var_names = { fuzz_helpers::getMetaVarName(0) };
        std::pair<std::string, std::string> rw_meta_rel =
            concretizeMetaRelation(mgi);
        mt_body << rw_meta_rel.first;
        this->rw.InsertText(test_main_fd->getBeginLoc(), rw_meta_rel.second);
    }
    return mt_body.str();
}

std::pair<std::string, std::string>
metaGenerator::concretizeMetaRelation(mrGenInfo& mgi)
{
    clang::Rewriter tmp_rw(this->rw.getSourceMgr(), this->rw.getLangOpts());
    std::stringstream test_ss, funcs_ss;

    if (mgi.mr_decl->mr_type != CHECK)
    {
        if (mgi.first_decl)
        {
            test_ss << meta_input_var_type_name << ' ';
            mgi.first_decl = false;
        }
        test_ss << mgi.curr_mr_var_name << " = ";
    }

    std::vector params = mgi.family_idx != -1
        ?  mr_input_vars.at(mgi.family_idx)
        : std::vector<std::string>();
    test_ss << makeMRFuncCall(mgi, nullptr, params) << ";" << std::endl;
    makeRecursiveFunctionCalls(mgi, funcs_ss);

    return std::make_pair(test_ss.str(), funcs_ss.str());
}

std::string
metaGenerator::makeUniqueFuncCallName(mrGenInfo& mgi)
{
    if (!mgi.mr_decl->is_base_func || mgi.mr_decl->has_fuzz_call)
    {
        return mgi.mr_decl->base_func->getNameAsString() + "_" +
            std::to_string(mgi.test_idx) + "_" + std::to_string(mgi.recursive_idx);
    }
    std::vector<std::string> call_with_namespaces =
        { this->mr_namespace_name, mr_type_string_map.at(mgi.mr_decl->mr_type) };
    if (!mgi.mr_decl->mr_family.empty())
    {
        call_with_namespaces.push_back(mgi.mr_decl->mr_family);
    }
    call_with_namespaces.push_back(mgi.mr_decl->mr_name);
    return std::accumulate(
        std::next(std::begin(call_with_namespaces)), std::end(call_with_namespaces),
        call_with_namespaces.front(), [](std::string& result, std::string& elem)
        {
            std::string namespace_delim = "::";
            return result + namespace_delim + elem;
        });
}

std::string
metaGenerator::makeMRFuncCall(mrGenInfo& mgi, mrInfo* calling_mr,
    std::vector<std::string> base_params, bool recursive)
{
    std::stringstream mr_func_call;
    std::string new_func_call_name = this->makeUniqueFuncCallName(mgi);
    mr_func_call << new_func_call_name << "(";
    size_t param_idx = 0;
    bool first_input_var = true;
    for (const clang::ParmVarDecl* pvd : mgi.mr_decl->base_func->parameters())
    {
        std::string param_name = "";
        if (recursive)
        {
            assert(calling_mr);
            if (param_idx < base_params.size())
            {
                param_name = base_params.at(param_idx);
            }
            else
            {
                param_name = retrieveMRDeclVar(calling_mr, pvd->getType().getTypePtr());
            }
        }
        else if (!base_params.empty())
        {
            assert(param_idx < base_params.size());
            param_name = base_params.at(param_idx);
            param_name = ((param_name == "<>") ? mgi.curr_mr_var_name : param_name);
        }
        else
        {
            assert(!calling_mr);
            assert(base_params.empty());
            clang::QualType pvt =
                pvd->getType()->isReferenceType()
                    ? pvd->getType()->getPointeeType()
                    : pvd->getType();

            //std::cout << "===== PVD =====" << std::endl;
            //pvd->dump();
            //std::cout << "===== PVT =====" << std::endl;
            //pvt->dump();
            //std::cout << "===== MIVT =====" << std::endl;
            //meta_input_var_type->dump();

            if (pvt.getTypePtr() == meta_input_var_type)
            {
                if (first_input_var && mgi.family_idx != 0)
                {
                    param_name = mgi.curr_mr_var_name;
                    first_input_var = false;
                }
                else
                {
                    param_name = fuzzer::clang::getRandElem(mgi.input_var_names);
                }
            }
            else if (std::string pvt_name = pvt.getAsString();
                     main_var_decls_typed.count(pvt.getAsString()))
            {
                std::vector<std::string> candidate_var_names;
                std::transform(main_var_decls_typed.at(pvt_name).begin(),
                    main_var_decls_typed.at(pvt_name).end(),
                    std::back_inserter(candidate_var_names),
                    [](const clang::VarDecl* vd) -> std::string
                    {
                        return vd->getNameAsString();
                    });
                param_name = fuzzer::clang::getRandElem(candidate_var_names);
            }
            else if (pvt->isBuiltinType())
            {
                if (const clang::TypedefType* tdt = llvm::dyn_cast<clang::TypedefType>(pvt))
                {
                    pvt = tdt->desugar();
                }
                param_name = fuzz_helpers::getBuiltinRandStr(
                    llvm::dyn_cast<clang::BuiltinType>(pvt));
            }
            assert(!param_name.empty());
            if (mgi.family_idx != -1)
            {
                if (param_name == mgi.curr_mr_var_name)
                {
                    mr_input_vars.at(mgi.family_idx).push_back("<>");
                }
                else
                {
                    mr_input_vars.at(mgi.family_idx).push_back(param_name);
                }
            }
        }
        mr_func_call << param_name;
        if (param_idx < mgi.mr_decl->base_func->parameters().size() - 1)
        {
            mr_func_call << ", ";
        }
        param_idx += 1;
    }
    mr_func_call << ")";
    return mr_func_call.str();
}

void
metaGenerator::makeRecursiveFunctionCalls(mrGenInfo& mgi, std::stringstream& funcs_ss)
{
    clang::Rewriter tmp_rw(this->rw.getSourceMgr(), this->rw.getLangOpts());
    std::set<const clang::CallExpr*>::iterator it =
        mgi.mr_decl->recursive_calls.begin();

    std::string renamed_recursive_mr = this->makeUniqueFuncCallName(mgi);
    //mgi.mr_decl->mr_name + "_" +
        //std::to_string(mgi.test_idx) + "_" + std::to_string(mgi.recursive_idx);
    tmp_rw.ReplaceText(mgi.mr_decl->base_func->getNameInfo().getSourceRange(), renamed_recursive_mr);

    while (it != mgi.mr_decl->recursive_calls.end())
    {
        const clang::CallExpr* ce = (*it);

        // Parse `placeholder` call name and retrieve MR type
        const clang::FunctionDecl* fd_r = llvm::dyn_cast<clang::FunctionDecl>(ce->getDirectCallee());
        std::vector<std::string> splits;
        std::string fd_name(fd_r->getQualifiedNameAsString());
        std::string delim = "::";
        std::string recursive_mr_type, recursive_mr_family;
        size_t prev_pos = 0, next_pos = fd_name.find(delim);
        if (next_pos == std::string::npos)
        {
            recursive_mr_type = mgi.mr_decl->getType();
            recursive_mr_family = mgi.mr_decl->getFamily();
        }
        else
        {
            while (next_pos != std::string::npos)
            {
                splits.push_back(fd_name.substr(prev_pos, next_pos - prev_pos));
                prev_pos = next_pos + delim.length();
                next_pos = fd_name.find(delim, prev_pos);
            }
            recursive_mr_type   = splits[1];
            recursive_mr_family = splits[2];
        }

        bool base_chance;
        switch (globals::prune_option)
        {
            // Linear weighing
            case linear:
            {
                base_chance =
                    fuzzer::clang::generateRand(1, globals::meta_test_depth) < mgi.depth;
                break;
            }
            // Front-loaded weighing
            case logarithm:
            {
                base_chance =
                    (log(mgi.depth + 1) / log(globals::meta_test_depth + 1)) >
                        fuzzer::clang::generateRand(0.0, 1.0);
                break;
            }
            // Do not weigh towards base functions
            case noprune:
            {
                base_chance = globals::meta_test_depth < mgi.depth;
            }
        }

        mrInfo recursive_mr_func = retrieveRandMrDecl(recursive_mr_type,
            recursive_mr_family, base_chance);
        mgi.recursive_idx += 1;

        std::vector<std::string> mr_call_params;
        for (const clang::Expr* ce_arg : ce->arguments())
        {
            if (const clang::ImplicitCastExpr* ice_arg =
                    llvm::dyn_cast<clang::ImplicitCastExpr>(ce_arg))
            {
                if (const clang::IntegerLiteral* il_arg =
                        llvm::dyn_cast<clang::IntegerLiteral>(ice_arg->getSubExpr()))
                {
                    mr_call_params.push_back(il_arg->getValue().toString(10, true));
                    continue;
                }
            }
            //else if (const clang::BinaryOperator* bo_arg =
                    //llvm::dyn_cast<clang::BinaryOperator>(ce_arg))
            //{
                //mr_call_params.push_back(bo_arg->EvaluateKnownConstInt(this->ctx).toString(10));
                //continue;
            //}
            mr_call_params.push_back(this->rw.getRewrittenText(
                ce_arg->getSourceRange()));
        }

        mrInfo* curr_mr = mgi.mr_decl;
        mgi.setMR(&recursive_mr_func);
        tmp_rw.ReplaceText(ce->getSourceRange(),
            makeMRFuncCall(mgi, curr_mr, mr_call_params, true));

        mgi.depth += 1;
        makeRecursiveFunctionCalls(mgi, funcs_ss);
        mgi.setMR(curr_mr);
        mgi.depth -= 1;
        it++;
    }
    if (mgi.mr_decl->mr_type != REL_TYPE::CHECK &&
            (!mgi.mr_decl->is_base_func || mgi.mr_decl->has_fuzz_call))
    {
        funcs_ss << tmp_rw.getRewrittenText(mgi.mr_decl->base_func->getSourceRange()) << std::endl;
    }
}


void
mrGenInfo::setMR(mrInfo* new_mr_decl)
{
    this->mr_decl = new_mr_decl;
}

mrInfo::mrInfo(const clang::FunctionDecl* FD) : helperFnDeclareInfo(FD)
{
    if (this->is_empty())
    {
        return;
    }

    /* Parse qualified function name */
    // mrDeclName fields:
    //      0 > `metalib` namespace
    //      1 > `generators`/`relations`/`checks`
    //      2 > family
    //      3 > MR name
    std::string fd_name(FD->getQualifiedNameAsString()), delim("::");
    std::vector<std::string> mrDeclName;
    size_t curr = fd_name.find(delim), prv = 0;
    while (curr != std::string::npos)
    {
        mrDeclName.push_back(fd_name.substr(prv, curr - prv));
        prv = curr + delim.length();
        curr = fd_name.find(delim, prv);
    }
    mrDeclName.push_back(fd_name.substr(prv, curr - prv));

    /* Set MR identifiers */
    assert(mrDeclName.size() >= 3);
    assert(!mrDeclName.at(0).compare("metalib"));
    this->mr_type = mr_type_map.at(mrDeclName.at(1));
    if (mrDeclName.size() == 3)
    {
        this->mr_name = mrDeclName.at(2);
    }
    else
    {
        this->mr_family = mrDeclName.at(2);
        this->mr_name = mrDeclName.at(3);
    }

    /* Set meta var type if parsing a relation */
    if (meta_input_var_type == nullptr &&
            this->mr_type == REL_TYPE::RELATION)
    {
        meta_input_var_type = FD->getReturnType().getTypePtr();
        if (const clang::TypedefType* td = llvm::dyn_cast<clang::TypedefType>(meta_input_var_type))
        {
            meta_input_var_type_name = td->getDecl()->getNameAsString();
        }
        else
        {
            meta_input_var_type_name = meta_input_var_type->getCanonicalTypeInternal().getAsString();
        }

        //std::vector<std::string> keywords {"class", "struct"};
        //for (std::string kw : keywords)
        //{
            //size_t kw_pos = meta_input_var_type_name.find(kw);
            //while (kw_pos != std::string::npos)
            //{
                //meta_input_var_type_name = meta_input_var_type_name.replace(kw_pos,kw.length(), "");
                //kw_pos = meta_input_var_type_name.find(kw);
            //}
        //}
        fuzzer::clang::addLibType(meta_input_var_type_name);
        //if (const clang::ElaboratedType* et = llvm::dyn_cast<clang::ElaboratedType>(meta_input_var_type))
        //{
            //std::string type_keyword = clang::TypeWithKeyword::getKeywordName(et->getKeyword()).str();
            //size_t keyword_pos = meta_input_var_type_name.find(type_keyword);
            //CHECK_CONDITION(keyword_pos != std::string::npos,
                //"Could not find keyword " + type_keyword + " in return type " + meta_input_var_type_name + ".");
            //meta_input_var_type_name = meta_input_var_type_name.replace(keyword_pos, type_keyword.size(), "");
        //}
    }
    assert(this->mr_type != REL_TYPE::RELATION ||
        meta_input_var_type == FD->getReturnType().getTypePtr());

    /* Gather instructions and set return instruction */
    clang::CompoundStmt* cs = llvm::dyn_cast<clang::CompoundStmt>(
        FD->getBody());
    assert(cs);
    for (clang::Stmt* child : cs->children())
    {
        this->body_instrs.push_back(child);
    }
}

std::string
retrieveMRDeclVar(mrInfo* mri, const clang::Type* var_type)
{
    std::set<const clang::VarDecl*> vars = mri->body_vd;
    vars.insert(mri->base_func->param_begin(), mri->base_func->param_end());
    std::set<const clang::VarDecl*>::iterator it = vars.begin();
    while (it != vars.end())
    {
        if ((*it)->getType().getTypePtr() != var_type)
        {
            it = vars.erase(it);
        }
        else
        {
            it++;
        }
    }
    it = vars.begin();
    std::advance(it, fuzzer::clang::generateRand(0, vars.size() - 1));
    return (*it)->getNameAsString();
}

mrInfo
retrieveRandMrDecl(std::string mr_type_str, std::string family, bool base)
{
    if (!mr_type_str.compare("generators"))
    {
        return retrieveRandMrDecl(REL_TYPE::GENERATOR, family, base);
    }
    if (!mr_type_str.compare("relations"))
    {
        return retrieveRandMrDecl(REL_TYPE::RELATION, family, base);
    }
    assert(false);
}

mrInfo
retrieveRandMrDecl(REL_TYPE mr_type, std::string family, bool base)
{
    CHECK_CONDITION(meta_rel_decls.count(std::make_pair(mr_type, family)),
        "Could not find MR type " + mr_type_string_map.at(mr_type) +
        " for family " + family + " (base " + std::to_string(base) + ").");
    std::vector<mrInfo> matchingDecls = meta_rel_decls.at(std::make_pair(mr_type, family));
    if (base)
    {
        matchingDecls.erase(
            std::remove_if(std::begin(matchingDecls), std::end(matchingDecls),
            [](mrInfo mri) { return !mri.is_base_func; }),
                std::end(matchingDecls));
    }
    CHECK_CONDITION(!matchingDecls.empty(),
        "Could not find base MR for family " + family + ".");
    return matchingDecls.at(fuzzer::clang::generateRand(0, matchingDecls.size() - 1));
}

void
testMainLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* fd =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("mainDecl");
    assert(fd);
    if (test_main_fd)
    {
        assert(fd == test_main_fd);
    }
    else
    {
        test_main_fd = fd;
    }
    const clang::VarDecl* vd =
            Result.Nodes.getNodeAs<clang::VarDecl>("mainVarDecl");
    assert(vd);
    std::string vd_type_name = vd->getType().getAsString();
    if (!main_var_decls_typed.count(vd_type_name))
    {
        main_var_decls_typed.emplace(vd_type_name, std::vector<const clang::VarDecl*>());
    }
    main_var_decls_typed.at(vd_type_name).emplace_back(vd);
}

void
metaCallsLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::CallExpr* ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("metaTestCall");
    assert(ce);
    meta_test_calls.push_back(ce);
}

void
mrRecursiveLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* fd =
        Result.Nodes.getNodeAs<clang::FunctionDecl>("mrFuncDecl");
    const clang::CallExpr* ce =
        Result.Nodes.getNodeAs<clang::CallExpr>("mrRecursiveCall");
    assert(fd);
    if (ce)
    {
        if (!this->matched_recursive_calls.count(fd))
        {
            this->matched_recursive_calls.emplace(
                std::make_pair(fd, std::set<const clang::CallExpr*>()));
        }
        this->matched_recursive_calls.at(fd).emplace(ce);
    }
}

metaGenerator::metaGenerator(clang::Rewriter& _rw, clang::ASTContext& _ctx):
    rw(_rw), ctx(_ctx)
{
    mr_matcher.addMatcher(
        clang::ast_matchers::varDecl(
        clang::ast_matchers::hasParent(
        clang::ast_matchers::declStmt(
        clang::ast_matchers::hasParent(
        clang::ast_matchers::compoundStmt(
        clang::ast_matchers::hasParent(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::isMain())
            .bind("mainDecl")))))))
        .bind("mainVarDecl"), &main_logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::callExpr(
        clang::ast_matchers::callee(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasName(
        "fuzz::meta_test"))))
            .bind("metaTestCall"), &mc_logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))
            .bind("metaRel"), &mr_logger);
}

void
metaGenerator::HandleTranslationUnit(clang::ASTContext& ctx)
{
    mr_matcher.matchAST(ctx);
    for (const clang::FunctionDecl* fd : this->mr_logger.matched_fds)
    {
        if (fd->getName().equals(this->recursive_func_call_name))
        {
            continue;
        }
        this->logMetaRelDecl(fd);
    }
    this->expandMetaTests();
}

void
metaGenerator::logMetaRelDecl(const clang::FunctionDecl* fd)
{
    mrInfo new_mr_decl(fd);
    if (new_mr_decl.getType() != REL_TYPE::CHECK)
    {
        MRTraverser new_mr_traverser(new_mr_decl);
    }

    if (new_mr_decl.getFamily().empty())
    {
        meta_check_decls.push_back(new_mr_decl);
        return;
    }

    std::pair<REL_TYPE, std::string> mr_category(
        new_mr_decl.getType(), new_mr_decl.getFamily());
    if (!meta_rel_decls.count(mr_category))
    {
        meta_rel_decls.emplace(mr_category, std::vector<mrInfo>({new_mr_decl}));
    }
    else
    {
        meta_rel_decls.at(mr_category).push_back(new_mr_decl);
    }
}

void
metaGenerator::expandMetaTests()
{
    assert(meta_input_var_type != nullptr);
    std::vector<std::string> input_var_names;
    // TODO
    for (size_t i = 0; i < globals::meta_input_fuzz_count; ++i)
    {
        input_var_names.push_back(fuzz_helpers::getMetaInputVarName(i));
    }
    for (const clang::CallExpr* meta_call : meta_test_calls)
    {
        this->indent =
            clang::Lexer::getIndentationForLine(meta_call->getBeginLoc(),
                this->rw.getSourceMgr()).str();
        //meta_call->dump();
        //meta_call->getDirectCallee()->dump();

        this->rw.ReplaceText(meta_call->getSourceRange(),
            this->generateMetaTests(input_var_names));
    }
}

bool
metaGeneratorAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "metaGeneratorAction");
    return true;
}

void
metaGeneratorAction::EndSourceFileAction()
{
    std::error_code ec;
    int fd;
    llvm::sys::fs::createTemporaryFile("mtFuzz", "cpp", fd,
        globals::rewritten_input_file);
    llvm::raw_fd_ostream rif_rfo(fd, true);
    rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
}

std::unique_ptr<clang::ASTConsumer>
metaGeneratorAction::CreateASTConsumer(clang::CompilerInstance& CI,
    llvm::StringRef File)
{
    rw.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<metaGenerator>(rw, CI.getASTContext());
}
