#include "parseFuzzerCalls.hpp"

std::vector<const clang::CallExpr*> fuzzer_calls;

extern std::string output_file;

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
            assert(fd->getTemplateSpecializationArgs()->size() == 1);
            std::string rand_type = fd->getTemplateSpecializationArgs()->get(0)
                .getAsType().getAsString();
            std::string replace_val = "";
            if (!rand_type.compare("int"))
            {
                int min = 0, max = std::numeric_limits<int>::max();
                clang::CallExpr::const_arg_iterator it = ce->arg_begin();
                if (it != ce->arg_end())
                {
                    min = llvm::dyn_cast<clang::IntegerLiteral>(*it)
                        ->getValue().getSExtValue();
                    std::advance(it, 1);
                    if (it != ce->arg_end())
                    {
                        max = llvm::dyn_cast<clang::IntegerLiteral>(*it)
                            ->getValue().getSExtValue();
                        assert(std::next(it) == ce->arg_end());
                    }
                }

                replace_val = std::to_string(fuzzer::clang::generateRand(min, max));

            }
            assert(replace_val != "");
            rw.ReplaceText(ce->getSourceRange(), replace_val);
            continue;
        }
        assert(false);
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
    std::cout << "[parseFuzzerCallsAction] Parsing input file ";
    std::cout << ci.getSourceManager().getFileEntryForID(
        ci.getSourceManager().getMainFileID())->getName().str() << std::endl;
    return true;
}

void
parseFuzzerCallsAction::EndSourceFileAction()
{
    assert(!output_file.empty());
    std::error_code ec;
    llvm::raw_fd_ostream of_rfo(output_file, ec);
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
