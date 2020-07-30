#include "generateMetaTests.hpp"

static std::vector<const clang::VarDecl*> main_var_decls;
static std::vector<const clang::CallExpr*> meta_test_calls;
static const clang::FunctionDecl* test_main_fd;
std::string recursive_func_call_name = "placeholder";
std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
std::vector<mrInfo> meta_check_decls;

const clang::Type* meta_input_var_type = nullptr;
std::string meta_input_var_type_name = "";
std::string mr_vd_suffix = "_mrv";

std::map<std::string, REL_TYPE> mr_type_map {
    { "generators" , GENERATOR },
    { "relations" , RELATION },
    { "checks", CHECK },
};


std::string
generateMetaTests(std::vector<std::string> input_var_names,
    const clang::Type* meta_input_var_type, std::string indent, clang::Rewriter& rw)
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
    for (size_t i = 0; i < meta_test_rel_count; ++i)
    {
        std::set<std::string>::const_iterator it = meta_families.cbegin();
        std::advance(it, fuzzer::clang::generateRand(0, meta_families.size() - 1));
        meta_family_chain.push_back(*it);
    }
    std::stringstream meta_tests;
    meta_tests << '\n';

    // Generate single metamorphic test
    for (int i = 0; i < meta_test_count; ++i)
    {
        std::string meta_test = generateSingleMetaTest(input_var_names,
            meta_input_var_type, meta_family_chain, rw, i);
        meta_tests << "// Meta test " << i << std::endl;
        meta_tests << meta_test << std::endl;
    }
    //std::cout << " === META TESTS" << std::endl;
    //std::cout << meta_tests.str();
    return meta_tests.str();
}

std::string
generateSingleMetaTest(std::vector<std::string> input_var_names,
    const clang::Type* meta_input_var_type,
    const std::vector<std::string>& meta_family_chain, clang::Rewriter& rw,
    size_t test_count)
{
    std::stringstream mt_body;
    std::string curr_mr_var_name = meta_var_name + std::to_string(test_count);
    // TODO grab correct indent
    std::string indent = "\t";
    mrGenInfo mgi(curr_mr_var_name, input_var_names, test_count, rw);
    for (std::string meta_family : meta_family_chain)
    {
        std::vector<mrInfo> meta_rel_choices =
            meta_rel_decls.at(std::make_pair(REL_TYPE::RELATION, meta_family));
        mrInfo chosen_mr = meta_rel_choices.at(
            fuzzer::clang::generateRand(0, meta_rel_choices.size() - 1));
        mgi.setMR(&chosen_mr);

        std::pair<std::string, std::string> rw_meta_rel =
            concretizeMetaRelation(mgi);
        if (mgi.first_decl)
        {
            mgi.first_decl = false;
            mgi.input_var_names.at(0) = curr_mr_var_name;
        }
        mgi.recursive_idx += 1;

        mt_body << rw_meta_rel.first;
        rw.InsertText(test_main_fd->getBeginLoc(), rw_meta_rel.second);
        //std::cout << "ONE META TESTS" << std::endl;
        //std::cout << rw_meta_rel.first;
        //std::cout << "ONE RECURSIVE META FUNCTIONS" << std::endl;
        //std::cout << rw_meta_rel.second;
    }
    for (mrInfo meta_check : meta_check_decls)
    {
        mgi.setMR(&meta_check);
        mgi.input_var_names = { meta_var_name + "0" , curr_mr_var_name };
        mgi.curr_mr_var_name = "";
        std::pair<std::string, std::string> rw_meta_rel =
            concretizeMetaRelation(mgi);
        mt_body << rw_meta_rel.first;
        rw.InsertText(test_main_fd->getBeginLoc(), rw_meta_rel.second);
    }
    return mt_body.str();
}

std::pair<std::string, std::string>
concretizeMetaRelation(mrGenInfo& mgi)
{
    clang::Rewriter tmp_rw(mgi.rw.getSourceMgr(), mgi.rw.getLangOpts());
    std::stringstream test_ss, funcs_ss;

    if (!mgi.curr_mr_var_name.empty())
    {
        if (mgi.first_decl)
        {
            test_ss << meta_input_var_type_name << ' ';
        }
        test_ss << mgi.curr_mr_var_name << " = ";
    }

    test_ss << makeMRFuncCall(mgi) << ";" << std::endl;
    makeRecursiveFunctionCalls(mgi, funcs_ss);

    return std::make_pair(test_ss.str(), funcs_ss.str());
}

std::string
makeMRFuncCall(mrGenInfo& mgi, mrInfo* calling_mr,
    std::vector<std::string> base_params, bool recursive)
{
    std::stringstream mr_func_call;
    std::string new_func_call_name =
        mgi.mr_decl->base_func->getNameAsString() +
        std::to_string(mgi.test_idx) + "_" + std::to_string(mgi.recursive_idx);
    mr_func_call << new_func_call_name << "(";
    size_t param_idx = 0, implicit_param_counter = 0;
    for (const clang::ParmVarDecl* pvd : mgi.mr_decl->base_func->parameters())
    {
        if (recursive)
        {
            assert(calling_mr);
            //mr_func_call << pvd->getNameAsString() << std::endl;
            if (param_idx < base_params.size())
            {
                mr_func_call << base_params.at(param_idx);
            }
            else
            {
                mr_func_call << retrieveMRDeclVar(calling_mr, pvd->getType().getTypePtr());
                //mr_func_call << pvd->getNameAsString();
            }
        }
        else
        {
            assert(!calling_mr);
            const clang::QualType pvt =
                pvd->getType()->isReferenceType()
                ? pvd->getType()->getPointeeType()
                : pvd->getType();
            if (pvt.getTypePtr() == meta_input_var_type)
            {
                mr_func_call << mgi.input_var_names.at(implicit_param_counter);
                implicit_param_counter += 1;
            }
            else
            {
                bool found = false;
                //pvd->dump();
                for (const clang::VarDecl* vd : main_var_decls)
                {
                    if (pvt == vd->getType())
                    {
                        mr_func_call << vd->getNameAsString();
                        found = true;
                        break;
                    }
                }
                assert(found);
            }
        }
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
makeRecursiveFunctionCalls(mrGenInfo& mgi, std::stringstream& funcs_ss)
{
    clang::Rewriter tmp_rw(mgi.rw.getSourceMgr(), mgi.rw.getLangOpts());
    std::set<const clang::CallExpr*>::iterator it =
        mgi.mr_decl->recursive_calls.begin();

    std::string renamed_recursive_mr = mgi.mr_decl->mr_name +
        std::to_string(mgi.test_idx) + "_" + std::to_string(mgi.recursive_idx);
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

        mrInfo recursive_mr_func = retrieveRandMrDecl(recursive_mr_type,
            recursive_mr_family, mgi.depth > meta_test_depth);
        mgi.recursive_idx += 1;

        std::vector<std::string> mr_call_params;
        for (const clang::Expr* arg : ce->arguments())
        {
            mr_call_params.push_back(mgi.rw.getRewrittenText(arg->getSourceRange()));
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
    funcs_ss << tmp_rw.getRewrittenText(mgi.mr_decl->base_func->getSourceRange()) << std::endl;
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
    // TODO potentially unify metalib::checks namespace
    if (meta_input_var_type == nullptr &&
            FD->getQualifiedNameAsString().find("metalib::checks")
                == std::string::npos)
    {
        meta_input_var_type = FD->getReturnType().getTypePtr();
        meta_input_var_type_name = meta_input_var_type->getCanonicalTypeInternal().getAsString();

        size_t class_pos = meta_input_var_type_name.find("class");
        if (class_pos != std::string::npos)
        {
            meta_input_var_type_name = meta_input_var_type_name.replace(class_pos, sizeof("class"), "");
        }
        //if (const clang::ElaboratedType* et = llvm::dyn_cast<clang::ElaboratedType>(meta_input_var_type))
        //{
            //std::string type_keyword = clang::TypeWithKeyword::getKeywordName(et->getKeyword()).str();
            //size_t keyword_pos = meta_input_var_type_name.find(type_keyword);
            //CHECK_CONDITION(keyword_pos != std::string::npos,
                //"Could not find keyword " + type_keyword + " in return type " + meta_input_var_type_name + ".");
            //meta_input_var_type_name = meta_input_var_type_name.replace(keyword_pos, type_keyword.size(), "");
        //}
    }

    /* Parse qualified function name */
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

    /* Gather instructions and set return instruction */
    clang::CompoundStmt* cs = llvm::dyn_cast<clang::CompoundStmt>(
        FD->getBody());
    assert(cs);
    for (clang::Stmt* child : cs->children())
    {
        this->body_instrs.push_back(child);
    }

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

    //clang::QualType return_type = FD->getReturnType();
    //std::string return_type_str = return_type.getAsString();
    //if (const clang::ElaboratedType* et = llvm::dyn_cast<clang::ElaboratedType>(return_type))
    //{
        //std::string type_keyword = clang::TypeWithKeyword::getKeywordName(et->getKeyword()).str();
        //size_t keyword_pos = return_type_str.find(type_keyword);
        //CHECK_CONDITION(keyword_pos != std::string::npos,
            //"Could not find keyword " + type_keyword + " in return type " + return_type_str + ".");
        //return_type_str = return_type_str.replace(keyword_pos, type_keyword.size(), "");
    //}

    assert(this->isCheck() ||
        FD->getReturnType().getTypePtr() == meta_input_var_type);
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
    //vars.erase(std::remove_if(std::begin(vars), std::end(vars),
        //[&var_type](const clang::VarDecl* var)
        //{
            //return var->getType().getTypePtr() != var_type;
        //}));
    it = vars.begin();
    std::advance(it, fuzzer::clang::generateRand(0, vars.size() - 1));
    return (*it)->getNameAsString();
    //return vars.at(fuzzer::clang::generateRand(0, vars.size() - 1))->getNameAsString();
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
    std::vector<mrInfo> matchingDecls = meta_rel_decls.at(std::make_pair(mr_type, family));
    if (base)
    {
        matchingDecls.erase(std::remove_if(std::begin(matchingDecls),
            std::end(matchingDecls),
            [](mrInfo mri) { return !mri.is_base_func; }),
                std::end(matchingDecls));
    }
    assert(!matchingDecls.empty());
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
        assert(test_main_fd == test_main_fd);
    }
    else
    {
        test_main_fd = fd;
    }
    const clang::VarDecl* vd =
            Result.Nodes.getNodeAs<clang::VarDecl>("mainVarDecl");
    assert(vd);
    main_var_decls.push_back(vd);
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

    /*
    if (s)
    {
        s->dump();
        assert(ce);
        ce->dump();
        if (!this->matched_recursive_calls.count(fd))
        {
            this->matched_recursive_calls.emplace(
                std::make_pair(fd, std::vector<const clang::CallExpr*>>()));
            //this->matched_recursive_calls.emplace(
                //std::make_pair(fd,
                    //std::map<const clang::Stmt*, std::vector<const clang::CallExpr*>>()));
        }
        //if (!this->matched_recursive_calls.at(fd).count(s))
        //{
            //this->matched_recursive_calls.at(fd).emplace(
                //std::make_pair(s, std::vector<const clang::CallExpr*>()));
        //}
        this->matched_recursive_calls.at(fd).push_back(ce);
    }
    */
}

metaGenerator::metaGenerator(clang::Rewriter& _rw, clang::ASTContext& _ctx):
    rw(_rw), ctx(_ctx)
{
    mr_matcher.addMatcher(
        clang::ast_matchers::varDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::isMain())
            .bind("mainDecl")))
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

    mr_matcher.addMatcher(
        clang::ast_matchers::declRefExpr(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))
            .bind("mrFuncDecl")))
            .bind("mrDRE"), &mr_dre_logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::varDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))
            .bind("mrFuncDecl")))
            .bind("mrVD"), &mr_dre_logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::callExpr(
        clang::ast_matchers::allOf(
            clang::ast_matchers::callee(
            clang::ast_matchers::functionDecl(
            clang::ast_matchers::hasName(
            recursive_func_call_name))),
            clang::ast_matchers::hasAncestor(
            clang::ast_matchers::functionDecl(
            clang::ast_matchers::hasAncestor(
            clang::ast_matchers::namespaceDecl(
            clang::ast_matchers::hasName(
            "metalib"))))
                .bind("mrFuncDecl"))))
                .bind("mrRecursiveCall"), &mr_recursive_logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::compoundStmt(
        clang::ast_matchers::allOf(
            clang::ast_matchers::hasAnySubstatement(
            clang::ast_matchers::stmt(
            clang::ast_matchers::hasDescendant(
            clang::ast_matchers::callExpr(
            clang::ast_matchers::callee(
            clang::ast_matchers::functionDecl(
            clang::ast_matchers::hasName(
            recursive_func_call_name))))
                .bind("mrRecursiveCall")))
                .bind("mrRecursiveCallStmt")),
            clang::ast_matchers::hasAncestor(
            clang::ast_matchers::functionDecl(
            clang::ast_matchers::hasAncestor(
            clang::ast_matchers::namespaceDecl(
            clang::ast_matchers::hasName(
            "metalib"))))
                .bind("mrFuncDecl")))), &mr_recursive_logger);
}

void
metaGenerator::HandleTranslationUnit(clang::ASTContext& ctx)
{
    mr_matcher.matchAST(ctx);
    for (const clang::FunctionDecl* fd : this->mr_logger.matched_fds)
    {
        if (fd->getName().equals(recursive_func_call_name))
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
    this->mr_dre_matcher.match(*fd, ctx);
    new_mr_decl.body_dre = this->mr_dre_logger.matched_dres.at(fd);
    new_mr_decl.body_vd = this->mr_dre_logger.matched_vds.at(fd);
    if (this->mr_recursive_logger.matched_recursive_calls.count(fd))
    {
        new_mr_decl.recursive_calls =
            this->mr_recursive_logger.matched_recursive_calls.at(fd);
        new_mr_decl.is_base_func = false;
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
    for (size_t i = 0; i < meta_input_fuzz_count; ++i)
    {
        input_var_names.push_back(meta_input_var_prefix + std::to_string(i));
    }
    for (const clang::CallExpr* meta_call : meta_test_calls)
    {
        const std::string indent =
            clang::Lexer::getIndentationForLine(meta_call->getBeginLoc(),
                rw.getSourceMgr()).str();
        //meta_call->dump();
        //meta_call->getDirectCallee()->dump();

        rw.ReplaceText(meta_call->getSourceRange(),
            generateMetaTests(input_var_names, meta_input_var_type, indent, rw));
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
        rewritten_input_file);
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
