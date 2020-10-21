#include "helperFuncStitch.hpp"

static std::map<const clang::FunctionDecl*, helperFnDeclareInfo> helper_funcs_splits;
static std::vector<helperFnReplaceInfo> stitch_exprs;

static size_t replace_index = 0;
static const clang::CompoundStmt* main_child = nullptr;

const clang::ast_type_traits::DynTypedNode
getBaseParent(const clang::ast_type_traits::DynTypedNode dyn_node,
    clang::ASTContext& ctx)
{
    clang::ASTContext::DynTypedNodeList node_parents = ctx.getParents(dyn_node);
    assert(node_parents.size() == 1);
    if (main_child && node_parents[0].get<clang::CompoundStmt>() == main_child)
    {
        return dyn_node;
    }
    // TODO check correctly against list of MR function_decls
    else if (node_parents[0].get<clang::FunctionDecl>())
    {
        return dyn_node;
    }
    return getBaseParent(node_parents[0], ctx);
}

const clang::Stmt*
getBaseParent(const clang::Expr* e, clang::ASTContext& ctx)
{
    clang::ASTContext::DynTypedNodeList parents = ctx.getParents(*e);
    assert(parents.size() == 1);
    const clang::Stmt* base_parent =
        getBaseParent(parents[0], ctx).get<clang::Stmt>();
    assert(base_parent);
    return base_parent;
}

void
addNewSplit(const clang::FunctionDecl* fd)
{
    std::vector<clang::Stmt*> instrs;
    clang::Stmt* return_instr = nullptr;
    helper_funcs_splits.insert(std::make_pair(fd, helperFnDeclareInfo(fd)));

    clang::CompoundStmt* cs = llvm::dyn_cast<clang::CompoundStmt>(
        fd->getBody());
    assert(cs);
    for (clang::Stmt* child : cs->children())
    {
        if (clang::ReturnStmt* return_instr_tmp =
                llvm::dyn_cast<clang::ReturnStmt>(child))
        {
            // TODO could handle multiple return instructions
            assert(!return_instr);
            return_instr = *(return_instr_tmp->child_begin());
            assert(std::next(return_instr_tmp->child_begin())
                 == return_instr_tmp->child_end());
        }
        else
        {
            instrs.push_back(child);
        }
    }
    helper_funcs_splits.at(fd).body_instrs = instrs;
    helper_funcs_splits.at(fd).return_body = return_instr;
}

static std::string
parseConcreteVar(const clang::Stmt* s)
{
    std::string val = "";
    if (const clang::DeclRefExpr* dre =
            llvm::dyn_cast<clang::DeclRefExpr>(s))
    {
        return dre->getDecl()->getNameAsString();
    }
    else if (const clang::IntegerLiteral* il =
            llvm::dyn_cast<clang::IntegerLiteral>(s))
    {
        return std::to_string(il->getValue().getSExtValue());
    }
    else if (const clang::StringLiteral* sl =
            llvm::dyn_cast<clang::StringLiteral>(s))
    {
        return "\"" + sl->getString().str() + "\"";
    }
    for (clang::ConstStmtIterator ch_it = s->child_begin();
        ch_it != s->child_end(); ++ch_it)
    {
        return parseConcreteVar(*ch_it);
    }
    s->dump();
    assert(false);
}

std::pair<std::string, std::string>
helperFnDeclareInfo::getSplitWithReplacements(
    std::map<const clang::ParmVarDecl*, const clang::Stmt*> concrete_vars,
    clang::Rewriter& rw, size_t index)
{
    for (const clang::DeclRefExpr* dre : this->body_dre)
    {
        if (const clang::ParmVarDecl* pvd =
            llvm::dyn_cast<clang::ParmVarDecl>(dre->getDecl());
            pvd && concrete_vars.count(pvd))
        {
            rw.ReplaceText(dre->getSourceRange(),
                parseConcreteVar(concrete_vars.at(pvd)));
        }
        else if (const clang::VarDecl* vd =
                llvm::dyn_cast<clang::VarDecl>(dre->getDecl()))
        {
            rw.InsertText(dre->getLocation().getLocWithOffset(
                vd->getName().size()), "_" + std::to_string(index));
        }
    }
    for (const clang::VarDecl* vd : this->body_vd)
    {
        rw.InsertText(vd->getLocation().getLocWithOffset(vd->getName().size()),
            "_" + std::to_string(index));
    }

    std::string rewritten_body = std::accumulate(
        this->body_instrs.begin(), this->body_instrs.end(),
        std::string(),
        [&rw](std::string acc, clang::Stmt* s)
        {
            const std::string indent = clang::Lexer::getIndentationForLine(
                s->getBeginLoc(), rw.getSourceMgr()).str();
            return acc + '\n' + indent +
                rw.getRewrittenText(s->getSourceRange()) + ';';
        });
    // TODO bug when rewritting the return body with a single DeclRefExpr;
    // the rewrite returned does not contain the added index identifier
    std::string rewritten_return =
        rw.getRewrittenText(this->return_body->getSourceRange());
    return std::make_pair(rewritten_body, rewritten_return);
}


helperFnReplaceInfo::helperFnReplaceInfo(const clang::CallExpr* _ce,
    const clang::Stmt* _base) : call_expr(_ce), base_stmt(_base)
{
    this->index = replace_index++;
    const clang::FunctionDecl* ce_callee =
        llvm::dyn_cast<clang::FunctionDecl>(_ce->getCalleeDecl());
    assert(ce_callee);
    helperFnDeclareInfo helper_info = helper_funcs_splits.at(ce_callee);
    clang::FunctionDecl::param_const_iterator helper_args_it =
        helper_info.base_func->param_begin();
    clang::CallExpr::const_arg_iterator call_args_it = _ce->arg_begin();
    for (size_t i = 0; i < _ce->getNumArgs(); ++i)
    {
        this->concrete_params.insert(std::make_pair(*(helper_args_it), *(call_args_it)));
        helper_args_it++;
        call_args_it++;
    }
}

void
fuzzHelperFuncReplacer::makeReplace(
    std::vector<helperFnReplaceInfo>& replace_exprs) const
{
    for (helperFnReplaceInfo info : replace_exprs)
    {
        const clang::FunctionDecl* ce_callee = info.call_expr->getDirectCallee();
        assert(ce_callee);

        clang::Rewriter tmp_rw(this->ctx.getSourceManager(),
                    this->ctx.getLangOpts());
        std::pair<std::string, std::string> replace_strs =
            helper_funcs_splits.at(ce_callee).getSplitWithReplacements(
                info.concrete_params, tmp_rw, info.index);

        rw.InsertText(info.base_stmt->getBeginLoc(), replace_strs.first + '\n' +
            clang::Lexer::getIndentationForLine(info.base_stmt->getBeginLoc(),
                rw.getSourceMgr()).str());
        rw.ReplaceText(info.call_expr->getSourceRange(), replace_strs.second);
    }
}

void
fuzzHelperFuncLocator::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::CompoundStmt* cs =
            Result.Nodes.getNodeAs<clang::CompoundStmt>("mainChild"))
    {
        assert(!main_child);
        main_child = cs;
    }
    else if (const clang::CallExpr* ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("helperFuncInvoke"))
    {
        stitch_exprs.emplace_back(ce, getBaseParent(ce, this->ctx));
    }
    else if (const clang::FunctionDecl* fd =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("helperFunc"))
    {
        if (!helper_funcs_splits.count(fd))
        {
            addNewSplit(fd);
        }
        if (const clang::DeclRefExpr* dre =
                Result.Nodes.getNodeAs<clang::DeclRefExpr>("helperFuncDRE"))
        {
            helper_funcs_splits.at(fd).body_dre.insert(dre);
            return;
        }
        else if (const clang::VarDecl* vd =
                Result.Nodes.getNodeAs<clang::VarDecl>("helperFuncVD"))
        {
            helper_funcs_splits.at(fd).body_vd.insert(vd);
            return;
        }
        assert(false);
    }
}

fuzzHelperFuncStitch::fuzzHelperFuncStitch(clang::Rewriter& _rw,
    clang::ASTContext& _ctx) : rw(_rw), locator(_ctx), replacer(_ctx, _rw)
{
    matcher.addMatcher(
        clang::ast_matchers::callExpr(
        clang::ast_matchers::callee(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "fuzz::lib_helper_funcs"))))))
            .bind("helperFuncInvoke"), &locator);

    matcher.addMatcher(
        clang::ast_matchers::varDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "fuzz::lib_helper_funcs"))))
            .bind("helperFunc")))
            .bind("helperFuncVD"), &locator);

    matcher.addMatcher(
        clang::ast_matchers::declRefExpr(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "fuzz::lib_helper_funcs"))))
            .bind("helperFunc")))
            .bind("helperFuncDRE"), &locator);

    matcher.addMatcher(
        clang::ast_matchers::compoundStmt(
        clang::ast_matchers::hasParent(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::isMain())))
            .bind("mainChild"), &locator);
}

void
fuzzHelperFuncStitch::HandleTranslationUnit(clang::ASTContext& ctx)
{
    matcher.matchAST(ctx);
    replacer.makeReplace(stitch_exprs);
}

bool
fuzzHelperFuncStitchAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "fuzzHelperFuncStitchAction");
    return true;
}

void
fuzzHelperFuncStitchAction::EndSourceFileAction()
{
    std::error_code ec;
    int fd;
    llvm::sys::fs::createTemporaryFile("mtFuzz", ".cpp", fd,
        globals::rewritten_input_file);
    llvm::raw_fd_ostream rif_rfo(fd, true);
    rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
    //llvm::sys::fs::remove(rewritten_input_file);

    //rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
        //.write(llvm::outs());
}

std::unique_ptr<clang::ASTConsumer>
fuzzHelperFuncStitchAction::CreateASTConsumer(clang::CompilerInstance& ci,
    llvm::StringRef file)
{
    rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<fuzzHelperFuncStitch>(rw, ci.getASTContext());
}
