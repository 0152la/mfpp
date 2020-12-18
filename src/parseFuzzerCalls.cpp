#include "parseFuzzerCalls.hpp"

std::vector<const clang::CallExpr*> fuzzer_calls;

int
fuzzerCallsReplacer::getIntFromClangExpr(
    clang::CallExpr::const_arg_iterator ce_it) const
{
    const clang::Expr* lit_expr = nullptr;
    const clang::UnaryOperator* uo =
            llvm::dyn_cast<const clang::UnaryOperator>(*ce_it);
    if (uo)
    {
        assert(uo->getOpcode() == clang::UnaryOperatorKind::UO_Minus);
        lit_expr = uo->getSubExpr();
    }
    else if (const clang::ImplicitCastExpr* ice =
            llvm::dyn_cast<const clang::ImplicitCastExpr>(*ce_it))
    {
        lit_expr = ice->getSubExpr();
    }
    else
    {
        lit_expr = *ce_it;
    }


    int int_val;
    if (const clang::BinaryOperator* bo = llvm::dyn_cast<const clang::BinaryOperator>(lit_expr))
    {
        llvm::APSInt int_const = bo->EvaluateKnownConstInt(this->ctx);
        int_val = int_const.getExtValue();
    }
    else if (const clang::IntegerLiteral* int_lit =
            llvm::dyn_cast<const clang::IntegerLiteral>(lit_expr))
    {
        int_val = int_lit->getValue().getSExtValue();
    }
    else
    {
        assert(false);
    }

    if (uo)
    {
        int_val = -int_val;
    }
    return int_val;
}

double
fuzzerCallsReplacer::getDoubleFromClangExpr(
    clang::CallExpr::const_arg_iterator ce_it) const
{
    const clang::Expr* e = *ce_it;
    const clang::UnaryOperator* uo =
            llvm::dyn_cast<const clang::UnaryOperator>(*ce_it);
    if (const clang::UnaryOperator* uo =
            llvm::dyn_cast<clang::UnaryOperator>(e))
    {
        assert(uo->getOpcode() == clang::UnaryOperatorKind::UO_Minus);
        e = uo->getSubExpr();
    }
    while (const clang::CastExpr* cast_e = llvm::dyn_cast<clang::CastExpr>(e))
    {
        e = cast_e->getSubExpr();
    }

    const clang::FloatingLiteral* fp_lit = llvm::dyn_cast<clang::FloatingLiteral>(e);
    const clang::IntegerLiteral* int_lit = llvm::dyn_cast<clang::IntegerLiteral>(e);
    assert(fp_lit || int_lit);

    if (fp_lit)
    {
        double fp_val = fp_lit->getValue().convertToDouble();
        if (uo)
        {
            fp_val = -fp_val;
        }
        return fp_val;
    }
    if (int_lit)
    {
        double int_val = int_lit->getValue().getZExtValue();
        if (uo)
        {
            int_val = -int_val;
        }
        return int_val;
    }
    assert(false);
}

void
fuzzerCallsReplacer::makeReplace(
    std::vector<const clang::CallExpr*>& replace_exprs) const
{
    for (const clang::CallExpr* ce : replace_exprs)
    {
        const clang::FunctionDecl* fd = ce->getDirectCallee();
        assert(fd);
        if (!fd->getNameAsString().compare("fuzz_rand"))
        {
            // TODO handle second argument type
            assert(fd->getTemplateSpecializationArgs()->size() == 2);
            std::string rand_type = fd->getTemplateSpecializationArgs()->get(0)
                .getAsType().getAsString();
            std::string replace_val = "";
            if (!rand_type.compare("int"))
            {
                int min = 0, max = std::numeric_limits<int>::max();
                clang::CallExpr::const_arg_iterator it = ce->arg_begin();
                if (it != ce->arg_end())
                {
                    min = fuzzerCallsReplacer::getIntFromClangExpr(it);
                    std::advance(it, 1);
                    if (it != ce->arg_end())
                    {
                        max = fuzzerCallsReplacer::getIntFromClangExpr(it);
                        //max = llvm::dyn_cast<clang::IntegerLiteral>(*it)
                            //->getValue().getSExtValue();
                    }
                    assert(std::next(it) == ce->arg_end());
                }

                replace_val = std::to_string(fuzzer::clang::generateRand(min, max));
            }
            else if (!rand_type.compare("double"))
            {
                double min = 0, max = std::numeric_limits<double>::max();
                clang::CallExpr::const_arg_iterator it = ce->arg_begin();
                if (it != ce->arg_end())
                {
                    min = fuzzerCallsReplacer::getDoubleFromClangExpr(it);
                    std::advance(it, 1);
                    if (it != ce->arg_end())
                    {
                        max = fuzzerCallsReplacer::getDoubleFromClangExpr(it);
                        //max = llvm::dyn_cast<clang::IntegerLiteral>(*it)
                            //->getValue().getSExtValue();
                    }
                    assert(std::next(it) == ce->arg_end());
                }

                replace_val = std::to_string(fuzzer::clang::generateRand(min, max));
            }
            else if (rand_type.find("basic_string") != std::string::npos)
            {
                uint8_t min = 0, max = std::numeric_limits<uint8_t>::max();
                clang::CallExpr::const_arg_iterator it = ce->arg_begin();
                if (it != ce->arg_end())
                {
                    min = static_cast<uint8_t>(fuzzerCallsReplacer::getDoubleFromClangExpr(it));
                    std::advance(it, 1);
                    if (it != ce->arg_end())
                    {
                        max = static_cast<uint8_t>(fuzzerCallsReplacer::getDoubleFromClangExpr(it));
                        //max = llvm::dyn_cast<clang::IntegerLiteral>(*it)
                            //->getValue().getSExtValue();
                    }
                    assert(std::next(it) == ce->arg_end());
                }

                replace_val = fuzzer::clang::generateRandStr(min, max);
            }
            if (!rand_type.compare("unsigned int"))
            {
                int min = 0, max = std::numeric_limits<int>::max();
                clang::CallExpr::const_arg_iterator it = ce->arg_begin();
                if (it != ce->arg_end())
                {
                    min = fuzzerCallsReplacer::getIntFromClangExpr(it);
                    std::advance(it, 1);
                    if (it != ce->arg_end())
                    {
                        max = fuzzerCallsReplacer::getIntFromClangExpr(it);
                        //max = llvm::dyn_cast<clang::IntegerLiteral>(*it)
                            //->getValue().getSExtValue();
                    }
                    assert(std::next(it) == ce->arg_end());
                }
                assert(min >= 0 && max >= 0);
                replace_val = std::to_string(fuzzer::clang::generateRand(min, max));
            }
            else
            {
                std::cout << "Random generation for type " << rand_type << " not implemented!" << std::endl;
                assert(false);
            }
            assert(replace_val != "");
            rw.ReplaceText(ce->getSourceRange(), replace_val);
            continue;
        }
        //assert(false);
    }
}

void
fuzzerCallsLocator::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::CallExpr* ce =
            Result.Nodes.getNodeAs<clang::CallExpr>("fuzzerCall"))
    {
        fuzzer_calls.push_back(ce);
    }
}

parseFuzzerCalls::parseFuzzerCalls(clang::Rewriter& _rw,
    clang::ASTContext& _ctx) : rw(_rw), locator(_ctx), replacer(_ctx, _rw)
{
    matcher.addMatcher(
        clang::ast_matchers::callExpr(
        clang::ast_matchers::callee(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "fuzz")))))).
            bind("fuzzerCall"), &locator);
}

void
parseFuzzerCalls::HandleTranslationUnit(clang::ASTContext& ctx)
{
    matcher.matchAST(ctx);
    replacer.makeReplace(fuzzer_calls);
}

bool
parseFuzzerCallsAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "parseFuzzerCallsAction");
    return true;
}

void
parseFuzzerCallsAction::EndSourceFileAction()
{
    assert(!globals::output_file.empty());
    std::error_code ec;
    llvm::raw_fd_ostream of_rfo(globals::output_file, ec);
    rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
        .write(of_rfo);
    of_rfo.close();

    //rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
        //.write(llvm::outs());
}

std::unique_ptr<clang::ASTConsumer>
parseFuzzerCallsAction::CreateASTConsumer(clang::CompilerInstance& ci,
    llvm::StringRef file)
{
    rw.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<parseFuzzerCalls>(rw, ci.getASTContext());
}
