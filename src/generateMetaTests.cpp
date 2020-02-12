#include "generateMetaTests.hpp"

static std::vector<const clang::CallExpr*> meta_test_calls;
std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
std::string meta_input_var_type = "";
std::string mr_vd_suffix = "_mrv";
std::vector<std::string> meta_input_var_names;

extern std::string meta_var_name;
extern size_t meta_input_fuzz_count;
extern size_t meta_test_rel_count;
extern size_t meta_test_count;
extern std::string meta_input_var_prefix;

static size_t mr_vd_rw_index = 0;

extern llvm::SmallString<256> rewritten_input_file;

std::map<std::string, REL_TYPE> mr_type_map {
    { "generators" , GENERATOR },
    { "relations" , RELATION },
};


std::string
generateMetaTests(std::vector<std::string> input_var_names,
    std::string meta_input_var_type, std::string indent, clang::Rewriter& rw)
{
    std::vector<std::string> meta_family_chain;
    std::set<std::string> meta_families;

    // Populate metamorphic input variables
    for (size_t i = 0; i < meta_input_fuzz_count; ++i)
    {
        meta_input_var_names.push_back(meta_input_var_prefix + std::to_string(i));
    }

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
        //meta_tests << std::replace(std::begin(meta_test), std::end(meta_test),
            //"\n", "\n" + indent)
    }
    //std::cout << " === META TESTS" << std::endl;
    //std::cout << meta_tests.str();
    return meta_tests.str();
}

std::string
generateSingleMetaTest(std::vector<std::string> input_var_names,
    std::string meta_input_var_type,
    const std::vector<std::string>& meta_family_chain, clang::Rewriter& rw,
    size_t test_count)
{
    std::stringstream mt_body;
    // TODO grab correct indent
    std::string indent = "\t";
    bool first_decl = true;
    for (std::string meta_family : meta_family_chain)
    {
        std::string curr_mr_var_name = meta_var_name + std::to_string(test_count);
        std::vector<mrInfo> meta_rel_choices =
            meta_rel_decls.at(std::make_pair(REL_TYPE::RELATION, meta_family));
        mrInfo chosen_mr = meta_rel_choices.at(
            fuzzer::clang::generateRand(0, meta_rel_choices.size() - 1));
        //std::pair<std::string, std::string> rw_meta_rel =
        mt_body <<
            concretizeMetaRelation(chosen_mr, rw, curr_mr_var_name, first_decl);
        first_decl = false;
        //mt_body << rw_meta_rel.first << '\n' << indent;
        //if (first_mr)
        //{
            //mt_body << meta_input_var_type << " ";
        //}
        //mt_body << curr_mr_var_name << " = " << rw_meta_rel.second << std::endl;
    }
    return mt_body.str();
}

//std::pair<std::string, std::string>
std::string
concretizeMetaRelation(helperFnDeclareInfo meta_rel_decl,
    clang::Rewriter& rw, std::string return_var_name, bool first_decl)
{
    std::string rw_body, rw_return;
    clang::Rewriter tmp_rw(rw.getSourceMgr(), rw.getLangOpts());
    for (const clang::VarDecl* vd : meta_rel_decl.body_vd)
    {
        tmp_rw.InsertText(
            vd->getLocation().getLocWithOffset(vd->getName().size()),
            std::to_string(mr_vd_rw_index));
    }
    for (const clang::DeclRefExpr* dre : meta_rel_decl.body_dre)
    {
        if (const clang::ParmVarDecl* pvd_dre = llvm::dyn_cast<clang::ParmVarDecl>(dre->getDecl()))
        {
            tmp_rw.ReplaceText(dre->getSourceRange(), meta_input_var_names.at(2));
            continue;
        }
        else if (const clang::VarDecl* vd_dre = llvm::dyn_cast<clang::VarDecl>(dre->getDecl()))
        {
            tmp_rw.InsertText(
                dre->getLocation().getLocWithOffset(vd_dre->getName().size()),
                std::to_string(mr_vd_rw_index));
            continue;
        }
        assert(false);
    }

    std::stringstream rw_str;
    // TODO Limitation: seems this rewritter thing doesn't like if variables are
    // used alone; cuts off the difference in length
    for (clang::Stmt* s : meta_rel_decl.body_instrs)
    {
        const std::string indent = clang::Lexer::getIndentationForLine(
            s->getBeginLoc(), tmp_rw.getSourceMgr()).str();
        rw_str << '\n' << indent;
        if (clang::ReturnStmt* rs = llvm::dyn_cast<clang::ReturnStmt>(s))
        {
            assert(std::next(rs->child_begin()) == rs->child_end());
            clang::Stmt* rs_body = *(rs->child_begin());
            if (first_decl)
            {
                rw_str << meta_input_var_type << ' ';
            }
            rw_str << return_var_name << " = " <<
                tmp_rw.getRewrittenText(rs_body->getSourceRange()) << ';';
        }
        else
        {
            rw_str << tmp_rw.getRewrittenText(s->getSourceRange()) << ';';
        }
    }
    std::cout << rw_str.str() << std::endl;
    mr_vd_rw_index += 1;
    return rw_str.str();

    //std::cout << " === CONCRETE DONE" << std::endl;
    //rw_body = std::accumulate(
        //std::begin(meta_rel_decl.body_instrs),
        //std::end(meta_rel_decl.body_instrs), std::string(),
        //[&tmp_rw](std::string acc, clang::Stmt* s)
        //{
            //const std::string indent = clang::Lexer::getIndentationForLine(
                //s->getBeginLoc(), tmp_rw.getSourceMgr()).str();
            //if (llvm::dyn_cast<clang::ReturnStmt>(s))
            //{
                //return acc + curr_mr_var_name << " = "
            //}
            //return acc + '\n' + indent +
                //tmp_rw.getRewrittenText(s->getSourceRange()) + ';';
        //});
    //meta_rel_decl.return_body->dump();
    //std::cout << tmp_rw.getRewrittenText(meta_rel_decl.return_body->getSourceRange()) << std::endl;
    //exit(1);
    //rw_return =
        //tmp_rw.getRewrittenText(meta_rel_decl.return_body->getSourceRange()) + ';';
    //return std::make_pair(rw_body, rw_return);

    //clang::Rewriter tmp_rw(ctx.getSourceManager(), ctx.getLangOpts());
    //helperFnReplaceInfo replace_info(caller, getBaseParent(caller, ctx));
    //std::pair<std::string, std::string> replace_strs =
        //meta_rel_decl.getSplitWithReplacements(
            //replace_info.concrete_params, tmp_rw, test_cnt);

    //std::cout << replace_strs.first << std::endl;
    //return replace_strs.first;
}

mrInfo::mrInfo(const clang::FunctionDecl* FD) : helperFnDeclareInfo(FD)
{
    if (meta_input_var_type.empty())
    {
        meta_input_var_type = FD->getReturnType().getAsString();
    }
    assert(!FD->getReturnType().getAsString().compare(meta_input_var_type));

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
        //if (clang::ReturnStmt* return_instr_tmp =
                //llvm::dyn_cast<clang::ReturnStmt>(child))
        //{
            //// TODO could handle multiple return instructions
            ////assert(!this->return_body);
            ////this->return_body = *(return_instr_tmp->child_begin());
            ////assert(std::next(return_instr_tmp->child_begin())
                 ////== return_instr_tmp->child_end());
            //this->return_instrs.push_back(return_instr_tmp);
        //}
        //else
        //{
            this->body_instrs.push_back(child);
        //}
    }

    /* Set MR identifiers */
    assert(!mrDeclName.at(0).compare("metalib"));
    this->mr_type = mr_type_map.at(mrDeclName.at(1));
    this->mr_family = mrDeclName.at(2);
    this->mr_name = mrDeclName.at(3);
}

mrInfo
retrieveRandMrDecl(REL_TYPE mr_type, std::string family)
{
    std::vector<mrInfo> matchingDecls = meta_rel_decls.at(std::make_pair(mr_type, family));
    return matchingDecls.at(fuzzer::clang::generateRand(0, matchingDecls.size() - 1));
}

void
metaCallsLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::CallExpr* ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("metaTestCall");
    assert(ce);
    meta_test_calls.push_back(ce);
}

metaGenerator::metaGenerator(clang::Rewriter& _rw, clang::ASTContext& _ctx):
    rw(_rw), ctx(_ctx)
{
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
        "metalib"))))))
            .bind("mrDRE"), &mr_dre_logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::varDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))))
            .bind("mrVD"), &mr_dre_logger);

    //mr_dre_matcher.addMatcher(
        //clang::ast_matchers::declRefExpr(
        //clang::ast_matchers::hasAncestor(
        //clang::ast_matchers::functionDecl()))
            //.bind("mrDRE"), &mr_dre_logger);

    //mr_dre_matcher.addMatcher(
        //clang::ast_matchers::declRefExpr()
            //.bind("fdTest"), &test_mcb);
}

void
metaGenerator::HandleTranslationUnit(clang::ASTContext& ctx)
{
    mr_matcher.matchAST(ctx);
    for (const clang::FunctionDecl* fd : this->mr_logger.matched_fds)
    {
        this->logMetaRelDecl(fd);
    }
    this->expandMetaTests();
}

void
metaGenerator::logMetaRelDecl(const clang::FunctionDecl* fd)
{
    mrInfo new_mr_decl(fd);
    this->mr_dre_matcher.match(*fd, ctx);
    new_mr_decl.body_dre = this->mr_dre_logger.matched_dres;
    new_mr_decl.body_vd = this->mr_dre_logger.matched_vds;

    //std::vector<const clang::DeclRefExpr*> new_mr_dres =
        //this->mr_dre_logger.matched_dres;
    //new_mr_decl.body_dre.insert(new_mr_decl.body_dre.end(), new_mr_dres.begin(),
        //new_mr_dres.end());


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
    //assert(!meta_input_var_type.isNull());
    assert(!meta_input_var_type.empty());
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
        //meta_call->dump();
        //meta_call->getDirectCallee()->dump();

        rw.ReplaceText(meta_call->getSourceRange(),
            generateMetaTests(input_var_names, meta_input_var_type, indent, rw));
    }
}

bool
metaGeneratorAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    std::cout << "[metaGeneratorAction] Parsing input file ";
    std::cout << ci.getSourceManager().getFileEntryForID(
        ci.getSourceManager().getMainFileID())->getName().str()
        << std::endl;
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
