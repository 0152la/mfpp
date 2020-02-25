#include "generateMetaTests.hpp"

static std::vector<const clang::CallExpr*> meta_test_calls;
static const clang::FunctionDecl* test_main_fd;
std::string recursive_func_call_name = "placeholder";
std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
std::string meta_input_var_type = "";
std::string mr_vd_suffix = "_mrv";

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
    size_t recursive_id = 0;
    for (std::string meta_family : meta_family_chain)
    {
        std::string curr_mr_var_name = meta_var_name + std::to_string(test_count);
        std::vector<mrInfo> meta_rel_choices =
            meta_rel_decls.at(std::make_pair(REL_TYPE::RELATION, meta_family));
        mrInfo chosen_mr = meta_rel_choices.at(
            fuzzer::clang::generateRand(0, meta_rel_choices.size() - 1));
        std::pair<std::string, std::string> rw_meta_rel =
            concretizeMetaRelation(chosen_mr, input_var_names, rw,
                curr_mr_var_name, first_decl, test_count, recursive_id);
        if (first_decl)
        {
            first_decl = false;
            input_var_names.at(0) = curr_mr_var_name;
        }
        recursive_id += 1;
        //mt_body << rw_meta_rel.first << '\n' << indent;
        //if (first_mr)
        //{
            //mt_body << meta_input_var_type << " ";
        //}
        //mt_body << curr_mr_var_name << " = " << rw_meta_rel.second << std::endl;

        mt_body << rw_meta_rel.first;
        rw.InsertText(test_main_fd->getBeginLoc(), rw_meta_rel.second);
        std::cout << "ONE META TESTS" << std::endl;
        std::cout << rw_meta_rel.first;
        std::cout << "ONE RECURSIVE META FUNCTIONS" << std::endl;
        std::cout << rw_meta_rel.second;
    }
    return mt_body.str();
}

std::pair<std::string, std::string>
concretizeMetaRelation(mrInfo meta_rel_decl, std::vector<std::string>& input_var_names,
    clang::Rewriter& rw, std::string return_var_name, bool first_decl,
    size_t test_id, size_t& recursive_id)
{
    clang::Rewriter tmp_rw(rw.getSourceMgr(), rw.getLangOpts());
    std::stringstream rw_str, recursive_decl_ss;

    if (!return_var_name.empty())
    {
        if (first_decl)
        {
            rw_str << meta_input_var_type << ' ';
        }
        rw_str << return_var_name << " = ";
    }
    rw_str << makeMRFuncCall(meta_rel_decl, test_id, recursive_id, input_var_names);
    makeRecursiveFunctionCalls(meta_rel_decl, rw, recursive_decl_ss, test_id, recursive_id, input_var_names);


    return std::make_pair(rw_str.str(), recursive_decl_ss.str());
}

std::string
makeMRFuncCall(mrInfo mr_decl, size_t test_idx, size_t recursive_idx, std::vector<std::string>& input_var_names, bool recursive)
{
    std::stringstream mr_func_call;
    std::string new_func_call_name =
        mr_decl.base_func->getNameAsString() +
        std::to_string(test_idx) + "_" + std::to_string(recursive_idx);
    mr_func_call << new_func_call_name << "(";
    size_t param_idx = 0;
    for (const clang::ParmVarDecl* pvd : mr_decl.base_func->parameters())
    {
        if (recursive)
        {
            mr_func_call << pvd->getNameAsString() << std::endl;

        } else
        {
            mr_func_call << input_var_names.at(param_idx);
        }
        if (param_idx < mr_decl.base_func->parameters().size() - 1)
        {
            mr_func_call << ", ";
        }
        param_idx += 1;
    }
    mr_func_call << ");" << std::endl;
    return mr_func_call.str();
}

void
makeRecursiveFunctionCalls(mrInfo mr_decl, clang::Rewriter& rw,
    std::stringstream& recursive_mr_ss, size_t test_idx, size_t& recursive_idx,
    std::vector<std::string> input_var_names)
{
    clang::Rewriter tmp_rw(rw.getSourceMgr(), rw.getLangOpts());
    std::map<const clang::Stmt*, std::vector<const clang::CallExpr*>>::iterator it =
        mr_decl.recursive_calls.begin();

    std::string renamed_recursive_mr = mr_decl.mr_name +
        std::to_string(test_idx) + "_" + std::to_string(recursive_idx);
    tmp_rw.ReplaceText(mr_decl.base_func->getNameInfo().getSourceRange(), renamed_recursive_mr);

    while (it != mr_decl.recursive_calls.end())
    {
        for (const clang::CallExpr* ce : (*it).second)
        {
            // Parse `placeholder` call name and retrieve MR type
            const clang::FunctionDecl* fd_r = llvm::dyn_cast<clang::FunctionDecl>(ce->getDirectCallee());
            std::vector<std::string> splits;
            std::string fd_name(fd_r->getQualifiedNameAsString());
            std::string delim = "::";
            size_t prev_pos = 0, next_pos = fd_name.find(delim);
            while (next_pos != std::string::npos)
            {
                splits.push_back(fd_name.substr(prev_pos, next_pos - prev_pos));
                prev_pos = next_pos + delim.length();
                next_pos = fd_name.find(delim, prev_pos);
            }

            mrInfo recursive_mr_func = retrieveRandMrDecl(splits[1], splits[2]);
            recursive_idx += 1;

            tmp_rw.ReplaceText(ce->getSourceRange(),
                makeMRFuncCall(recursive_mr_func, test_idx, recursive_idx, input_var_names, true));
            makeRecursiveFunctionCalls(recursive_mr_func, rw,
                recursive_mr_ss, test_idx, recursive_idx, input_var_names);
        }
        it++;
    }
    recursive_mr_ss << tmp_rw.getRewrittenText(mr_decl.base_func->getSourceRange()) << std::endl;
}

/*
std::pair<std::string, std::string>
//std::string
concretizeMetaRelation(mrInfo meta_rel_decl,
    clang::Rewriter& rw, std::string return_var_name, bool first_decl, size_t test_id)
{
    clang::Rewriter tmp_rw(rw.getSourceMgr(), rw.getLangOpts());
    // Disambiguate variable declarations
    for (const clang::VarDecl* vd : meta_rel_decl.body_vd)
    {
        tmp_rw.InsertText(
            vd->getLocation().getLocWithOffset(vd->getName().size()),
            std::to_string(mr_vd_rw_index));
    }

    // Correct references to disambiguated local variable declarations and
    // passed parameters
    for (const clang::DeclRefExpr* dre : meta_rel_decl.body_dre)
    {
        if (const clang::ParmVarDecl* pvd_dre = llvm::dyn_cast<clang::ParmVarDecl>(dre->getDecl())) {
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

    // Perform recursive calls
    //for (std::pair<const clang::Stmt*, std::vector<const clang::CallExpr*>> recursive_call_locs :
            //meta_rel_decl.recursive_calls)
    //{
        //for (const clang::CallExpr* ce : recursive_call_locs.second)
        //{
            //tmp_rw.ReplaceText(ce->getSourceRange(), "hello!");
        //}
    //}

    std::stringstream rw_str, recursive_decl_ss;
    size_t recursive_id = 0;
    // TODO Limitation: seems this rewritter thing doesn't like if variables are
    // used alone; cuts off the difference in length
    for (clang::Stmt* s : meta_rel_decl.body_instrs)
    {
        const std::string indent = clang::Lexer::getIndentationForLine(
            s->getBeginLoc(), tmp_rw.getSourceMgr()).str();
        rw_str << '\n' << indent;
        if (meta_rel_decl.recursive_calls.count(s))
        {
            for (const clang::CallExpr* ce : meta_rel_decl.recursive_calls.at(s))
            {
                const clang::FunctionDecl* fd_r = llvm::dyn_cast<clang::FunctionDecl>(ce->getDirectCallee());
                std::vector<std::string> splits;
                std::string fd_name(fd_r->getQualifiedNameAsString());
                std::string delim = "::";
                size_t prev_pos = 0, next_pos = fd_name.find(delim);
                while (next_pos != std::string::npos)
                {
                    splits.push_back(fd_name.substr(prev_pos, next_pos - prev_pos));
                    prev_pos = next_pos + delim.length();
                    next_pos = fd_name.find(delim, prev_pos);
                }
                std::string recursive_call =
                    generateRecursiveMRChain(
                        retrieveRandMrDecl(splits[1], splits[2]),
                        recursive_decl_ss, recursive_id, test_id, tmp_rw);
                recursive_id += 1;
                //recursive_decl_ss << recursive_call << std::endl;
                tmp_rw.ReplaceText(ce->getSourceRange(), recursive_call);
            }
            rw_str << tmp_rw.getRewrittenText(s->getSourceRange()) << ';';
        }
        else if (clang::ReturnStmt* rs = llvm::dyn_cast<clang::ReturnStmt>(s))
        {
            assert(std::next(rs->child_begin()) == rs->child_end());
            clang::Stmt* rs_body = *(rs->child_begin());
            std::string rs_body_str = tmp_rw.getRewrittenText(rs_body->getSourceRange());
            if (!return_var_name.empty())
            {
                if (first_decl)
                {
                    rw_str << meta_input_var_type << ' ';
                }
                rw_str << return_var_name << " = " << rs_body_str << ';';
            }
            else
            {
                //assert(calling_stmt);
                //rw_str << tmp_rw.getRewrittenText(calling_stmt->getSourceRange());
                //
                //tmp_rw.ReplaceText(calling_expr->getSourceRange(), rs_body_str);
                //rw_str << getRewrittenText(calling_stmt->getSourceRange());
            }
        }
        else
        {
            rw_str << tmp_rw.getRewrittenText(s->getSourceRange()) << ';';
        }
    }
    std::cout << rw_str.str() << std::endl;
    mr_vd_rw_index += 1;
    return std::make_pair<std::string, std::string>(rw_str.str(), recursive_decl_ss.str());
}
*/

std::string
generateRecursiveMRChain(const mrInfo& recursive_mr_decl,
    std::stringstream& recursive_decl_ss, size_t index, size_t test_id, clang::Rewriter& tmp_rw)
{
    size_t recursive_id = index;
    for (clang::Stmt* s : recursive_mr_decl.body_instrs)
    {
        if (recursive_mr_decl.recursive_calls.count(s))
        {
            for (const clang::CallExpr* ce : recursive_mr_decl.recursive_calls.at(s))
            {
                const clang::FunctionDecl* fd_r = llvm::dyn_cast<clang::FunctionDecl>(ce->getDirectCallee());
                std::vector<std::string> splits;
                std::string fd_name(fd_r->getQualifiedNameAsString());
                std::string delim = "::";
                size_t prev_pos = 0, next_pos = fd_name.find(delim);
                while (next_pos != std::string::npos)
                {
                    splits.push_back(fd_name.substr(prev_pos, next_pos - prev_pos));
                    prev_pos = next_pos + delim.length();
                    next_pos = fd_name.find(delim, prev_pos);
                }
                std::string recursive_call =
                    generateRecursiveMRChain(
                        retrieveRandMrDecl(splits[1], splits[2]),
                            recursive_decl_ss, recursive_id, test_id, tmp_rw);
                recursive_id += 1;
                //recursive_decl_ss << recursive_call << std::endl;
                tmp_rw.ReplaceText(ce->getSourceRange(), recursive_call);
            }
        }
        //recursive_decl_ss << tmp_rw.getRewrittenText(s->getSourceRange()) << ';';
    }
    std::string renamed_recursive_mr = recursive_mr_decl.mr_name +
        std::to_string(test_id) + "_" + std::to_string(index);
    tmp_rw.ReplaceText(recursive_mr_decl.base_func->getNameInfo().getSourceRange(), renamed_recursive_mr);
    recursive_decl_ss << tmp_rw.getRewrittenText(recursive_mr_decl.base_func->getSourceRange()) << std::endl;
    return renamed_recursive_mr;
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
        this->body_instrs.push_back(child);
    }

    /* Set MR identifiers */
    assert(!mrDeclName.at(0).compare("metalib"));
    this->mr_type = mr_type_map.at(mrDeclName.at(1));
    this->mr_family = mrDeclName.at(2);
    this->mr_name = mrDeclName.at(3);
}

mrInfo
retrieveRandMrDecl(std::string mr_type_str, std::string family)
{
    if (!mr_type_str.compare("generators"))
    {
        return retrieveRandMrDecl(REL_TYPE::GENERATOR, family);
    }
    if (!mr_type_str.compare("relations"))
    {
        return retrieveRandMrDecl(REL_TYPE::RELATION, family);
    }
    assert(false);
}

mrInfo
retrieveRandMrDecl(REL_TYPE mr_type, std::string family)
{
    std::vector<mrInfo> matchingDecls = meta_rel_decls.at(std::make_pair(mr_type, family));
    return matchingDecls.at(fuzzer::clang::generateRand(0, matchingDecls.size() - 1));
}

void
testMainLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* fd =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("testMain");
    assert(fd);
    assert(!test_main_fd);
    test_main_fd = fd;
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
    const clang::Stmt* s =
        Result.Nodes.getNodeAs<clang::Stmt>("mrRecursiveCallStmt");
    assert(fd);
    fd->dump();
    if (s)
    {
        s->dump();
        assert(ce);
        ce->dump();
        if (!this->matched_recursive_calls.count(fd))
        {
            this->matched_recursive_calls.emplace(
                std::make_pair(fd,
                    std::map<const clang::Stmt*, std::vector<const clang::CallExpr*>>()));
        }
        if (!this->matched_recursive_calls.at(fd).count(s))
        {
            this->matched_recursive_calls.at(fd).emplace(
                std::make_pair(s, std::vector<const clang::CallExpr*>()));
        }
        this->matched_recursive_calls.at(fd).at(s).push_back(ce);
    }
}

metaGenerator::metaGenerator(clang::Rewriter& _rw, clang::ASTContext& _ctx):
    rw(_rw), ctx(_ctx)
{
    mr_matcher.addMatcher(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::isMain())
            .bind("testMain"), &main_logger);

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

    //mr_matcher.addMatcher(
        //clang::ast_matchers::callExpr(
        //clang::ast_matchers::allOf(
        //clang::ast_matchers::callee(
        //clang::ast_matchers::functionDecl(
        //clang::ast_matchers::hasName(
            //recursive_func_call_name))),
        //clang::ast_matchers::hasAncestor(
        //clang::ast_matchers::functionDecl(
        //clang::ast_matchers::hasAncestor(
        //clang::ast_matchers::namespaceDecl(
        //clang::ast_matchers::hasName(
        //"metalib"))))
            //.bind("mrFuncDecl")),
        //clang::ast_))
            //.bind("mrRecursiveCall"), &mr_recursive_logger);

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
    new_mr_decl.body_dre = this->mr_dre_logger.matched_dres;
    new_mr_decl.body_vd = this->mr_dre_logger.matched_vds;
    if (this->mr_recursive_logger.matched_recursive_calls.count(fd))
    {
        new_mr_decl.recursive_calls =
            this->mr_recursive_logger.matched_recursive_calls.at(fd);
    }
    //new_mr_decl.calling_stmt =
    //new_mr_decl.calling_expr =

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
