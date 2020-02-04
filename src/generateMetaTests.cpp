#include "generateMetaTests.hpp"

static std::vector<const clang::CallExpr*> meta_test_calls;
static std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;

std::string meta_input_var_type;
extern size_t meta_test_rel_count;
extern size_t meta_input_fuzz_count;

std::map<std::string, REL_TYPE> mr_type_map {
    { "generators" , GENERATOR },
    { "relations" , RELATION },
};


std::string
generateMetaTests(std::vector<std::string> input_var_names,
    std::string meta_input_var_type, std::string indent)
{
    size_t meta_test_count = 20, meta_test_size = 5;
    std::vector<std::string> meta_family_chain;
    std::set<std::string> meta_families;
    std::for_each(std::begin(meta_rel_decls), std::end(meta_rel_decls),
        [&meta_families](std::pair<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> mr_decl)
        {
            meta_families.insert(mr_decl.first.second);
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
            meta_input_var_type, meta_family_chain);
        meta_tests << meta_test << std::endl;
        //meta_tests << std::replace(std::begin(meta_test), std::end(meta_test),
            //"\n", "\n" + indent)
    }
    std::cout << " === META TESTS" << std::endl;
    std::cout << meta_tests.str();
    exit(1);
    return meta_tests.str();
}

std::string
generateSingleMetaTest(std::vector<std::string> input_var_names,
    std::string meta_input_var_type, const std::vector<std::string>& meta_family_chain)
{
    for (std::string meta_family : meta_family_chain)
    {
        std::vector<mrInfo> meta_rel_choices = meta_rel_decls.at(std::make_pair(REL_TYPE::RELATION, meta_family));
        mrInfo chosen_mr = meta_rel_choices.at(fuzzer::clang::generateRand(0, meta_rel_choices.size() - 1));
        std::cout << " === CHOSEN MR " << std::endl;
        chosen_mr.base_func->dump();
    }
    exit(1);
}


std::string
concretizeMetaRelation(const clang::CallExpr* caller,
    helperFnDeclareInfo meta_rel_decl, size_t test_cnt, clang::ASTContext& ctx)
{
    clang::Rewriter tmp_rw(ctx.getSourceManager(), ctx.getLangOpts());
    helperFnReplaceInfo replace_info(caller, getBaseParent(caller, ctx));
    std::pair<std::string, std::string> replace_strs =
        meta_rel_decl.getSplitWithReplacements(
            replace_info.concrete_params, tmp_rw, test_cnt);

    std::cout << replace_strs.first << std::endl;
    return replace_strs.first;
    //rw.InsertText(replace_info.base_stmt->getBeginLoc(), replace_strs.first + '\n' +
        //clang::Lexer::getIndentationForLine(replace_info.base_stmt->getBeginLoc(),
            //rw.getSourceMgr()).str());
    //rw.ReplaceText(replace_info.call_expr->getSourceRange(), replace_strs.second);
}

mrInfo::mrInfo(const clang::FunctionDecl* FD) : helperFnDeclareInfo(FD)
{
    //std::cout << "MVIT ::: " << meta_input_var_type.getAsString() << std::endl;
    std::cout << "MVIT ::: " << meta_input_var_type << std::endl;
    //if (meta_input_var_type.empty())
    //{
        //meta_input_var_type = FD->getReturnType().getAsString();
        //std::cout << "ADDED" << std::endl;
        ////meta_input_var_type.dump();
    //}
    //assert(meta_input_var_type == FD->getReturnType());
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
    return matchingDecls.at(fuzzer::clang::generateRand(0, matchingDecls.size()));
}

void
logMetaRelDecl(const clang::FunctionDecl* fd)
{
    mrInfo new_mr_decl(fd);
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
    //std::vector<clang::Stmt*> instrs;
    //clang::Stmt* return_instr = nullptr;
    //meta_rel_decls.insert(std::make_pair(fd, helperFnDeclareInfo(fd)));

    //clang::CompoundStmt* cs = llvm::dyn_cast<clang::CompoundStmt>(
        //fd->getBody());
    //assert(cs);
    //for (clang::Stmt* child : cs->children())
    //{
        //if (clang::ReturnStmt* return_instr_tmp =
                //llvm::dyn_cast<clang::ReturnStmt>(child))
        //{
            //// TODO could handle multiple return instructions
            //assert(!return_instr);
            //return_instr = *(return_instr_tmp->child_begin());
            //assert(std::next(return_instr_tmp->child_begin())
                 //== return_instr_tmp->child_end());
        //}
        //else
        //{
            //instrs.push_back(child);
        //}
    //}
    //meta_rel_decls.at(fd).body_instrs = instrs;
    //meta_rel_decls.at(fd).return_body = return_instr;
}

void
metaRelsLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::CallExpr* ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("metaTestCall"))
    {
        meta_test_calls.push_back(ce);
    }
    else
    {
        const clang::FunctionDecl* FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("metaRel");
        assert(FD);
        logMetaRelDecl(FD);
    }
}

metaGenerator::metaGenerator(clang::Rewriter& _rw, clang::ASTContext& _ctx):
    rw(_rw), ctx(_ctx)
{
    mr_matcher.addMatcher(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))
            .bind("metaRel"), &logger);

    mr_matcher.addMatcher(
        clang::ast_matchers::callExpr(
        clang::ast_matchers::callee(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasName(
        "fuzz::meta_test"))))
            .bind("metaTestCall"), &logger);

}

void
metaGenerator::HandleTranslationUnit(clang::ASTContext& ctx)
{
    mr_matcher.matchAST(ctx);
    this->expandMetaTests();
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
        meta_call->dump();
        meta_call->getDirectCallee()->dump();

        rw.ReplaceText(meta_call->getSourceRange(),
            generateMetaTests(input_var_names, meta_input_var_type, indent));
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
