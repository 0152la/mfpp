#include "generateMetaTests.hpp"

static std::vector<const clang::CallExpr*> meta_test_calls;
std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
std::string meta_input_var_type = "";
std::vector<std::string> meta_input_var_names = {
    "metainvar1", "metainvar2", "metainvar3" };

extern size_t meta_test_rel_count;
extern size_t meta_input_fuzz_count;
extern std::string meta_input_var_prefix;

std::map<std::string, REL_TYPE> mr_type_map {
    { "generators" , GENERATOR },
    { "relations" , RELATION },
};


std::string
generateMetaTests(std::vector<std::string> input_var_names,
    std::string meta_input_var_type, std::string indent, clang::Rewriter& rw)
{
    size_t meta_test_count = 20, meta_test_size = 5;
    std::vector<std::string> meta_family_chain;
    std::set<std::string> meta_families;
    std::for_each(std::begin(meta_rel_decls), std::end(meta_rel_decls),
        [&meta_families](std::pair<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> mr_decl)
        {
            if (mr_decl.first.first == REL_TYPE::RELATION)
            {
                meta_families.insert(mr_decl.first.second);
            }
        });
    assert(!meta_families.empty());
    std::cout << " === META FAMILIES" << std::endl;
    for (std::string f : meta_families)
    {
        std::cout << f << std::endl;
    }
    for (size_t i = 0; i < meta_test_size; ++i)
    {
        std::set<std::string>::const_iterator it = meta_families.cbegin();
        std::advance(it, fuzzer::clang::generateRand(0, meta_families.size() - 1));
        meta_family_chain.push_back(*it);
    }
    std::stringstream meta_tests;
    meta_tests << '\n';
    for (int i = 0; i < meta_test_count; ++i)
    {
        std::string meta_test = generateSingleMetaTest(input_var_names,
            meta_input_var_type, meta_family_chain, rw);
        meta_tests << meta_test << std::endl;
        //meta_tests << std::replace(std::begin(meta_test), std::end(meta_test),
            //"\n", "\n" + indent)
    }
    std::cout << " === META TESTS" << std::endl;
    std::cout << meta_tests.str();
    return meta_tests.str();
}

std::string
generateSingleMetaTest(std::vector<std::string> input_var_names,
    std::string meta_input_var_type,
    const std::vector<std::string>& meta_family_chain, clang::Rewriter& rw)
{
    size_t index = 0;
    std::stringstream mt_body;
    for (std::string meta_family : meta_family_chain)
    {
        std::vector<mrInfo> meta_rel_choices =
            meta_rel_decls.at(std::make_pair(REL_TYPE::RELATION, meta_family));
        mrInfo chosen_mr = meta_rel_choices.at(
            fuzzer::clang::generateRand(0, meta_rel_choices.size() - 1));
        std::pair<std::string, std::string> rw_meta_rel =
            concretizeMetaRelation(chosen_mr, index, rw);
        ++index;

    }
    return mt_body.str();
}

std::pair<std::string, std::string>
concretizeMetaRelation(helperFnDeclareInfo meta_rel_decl, size_t test_cnt,
    clang::Rewriter& rw)
{
    std::string rw_body, rw_return;
    clang::Rewriter tmp_rw(rw.getSourceMgr(), rw.getLangOpts());
    for (const clang::DeclRefExpr* dre : meta_rel_decl.body_dre)
    {
        tmp_rw.ReplaceText(dre->getSourceRange(), meta_input_var_names.at(2));
    }
    std::cout << " === CONCRETE DONE" << std::endl;
    rw_body = std::accumulate(
        std::begin(meta_rel_decl.body_instrs),
        std::end(meta_rel_decl.body_instrs), std::string(),
        [&tmp_rw](std::string acc, clang::Stmt* s)
        {
            const std::string indent = clang::Lexer::getIndentationForLine(
                s->getBeginLoc(), tmp_rw.getSourceMgr()).str();
            return acc + '\n' + indent +
                tmp_rw.getRewrittenText(s->getSourceRange()) + ';';
        });
    rw_return = tmp_rw.getRewrittenText(meta_rel_decl.return_body->getSourceRange());
    std::cout << " INSTRS" << std::endl << rw_body << std::endl;
    std::cout << " RETURN" << std::endl << rw_return << std::endl;
    exit(1);
    return std::make_pair(rw_body, rw_return);

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
    std::vector<const clang::DeclRefExpr*> new_mr_dres =
        this->mr_dre_logger.matched_dres;
    new_mr_decl.body_dre.insert(new_mr_decl.body_dre.end(), new_mr_dres.begin(),
        new_mr_dres.end());
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
    //addMetaRels(*this->print_policy);
}

std::unique_ptr<clang::ASTConsumer>
metaGeneratorAction::CreateASTConsumer(clang::CompilerInstance& CI,
    llvm::StringRef File)
{
    rw.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<metaGenerator>(rw, CI.getASTContext());
}
