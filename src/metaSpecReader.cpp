#include "metaSpecReader.hpp"

extern std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
extern std::string meta_input_var_type;

void
mrDRELogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::VarDecl* VD = Result.Nodes.getNodeAs<clang::VarDecl>("mrVD"))
    {
        //std::cout << "VD DUMP" << std::endl;
        //VD->dump();
        this->matched_vds.push_back(VD);
        return;
    }
    else if (const clang::DeclRefExpr* DRE =
            Result.Nodes.getNodeAs<clang::DeclRefExpr>("mrDRE"))
    {
        if (llvm::dyn_cast<clang::ParmVarDecl>(DRE->getDecl()) ||
                llvm::dyn_cast<clang::VarDecl>(DRE->getDecl()))
        {
            //std::cout << "DRE DUMP" << std::endl;
            //DRE->dump();
            this->matched_dres.push_back(DRE);
        }
        return;
    }
    assert(false);
}

void
metaRelsLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("metaRel");
    assert(FD);
    this->matched_fds.push_back(FD);
}

metaRelsReader::metaRelsReader(clang::ASTContext& _ctx) : ctx(_ctx)
{
    mr_matcher.addMatcher(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))
            .bind("metaRel"), &mr_logger);

    mr_dre_matcher.addMatcher(
        clang::ast_matchers::declRefExpr(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::functionDecl()))
            .bind("mrDRE"), &dre_logger);
}

void
metaRelsReader::HandleTranslationUnit(clang::ASTContext& ctx)
{
    mr_matcher.matchAST(ctx);
    for (const clang::FunctionDecl* fd : this->mr_logger.matched_fds)
    {
        this->logMetaRelDecl(fd);
    }
}

void
metaRelsReader::logMetaRelDecl(const clang::FunctionDecl* fd)
{
    mrInfo new_mr_decl(fd);
    this->mr_dre_matcher.match(*fd, ctx);
    std::vector<const clang::DeclRefExpr*> new_mr_dres =
        this->dre_logger.matched_dres;
    new_mr_decl.body_dre.insert(new_mr_dres.end(), new_mr_dres.begin(),
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

bool
metaRelsReaderAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    std::cout << "[metaRelsReaderAction] Parsing input file ";
    std::cout << ci.getSourceManager().getFileEntryForID(
        ci.getSourceManager().getMainFileID())->getName().str()
        << std::endl;
    return true;
}

std::unique_ptr<clang::ASTConsumer>
metaRelsReaderAction::CreateASTConsumer(clang::CompilerInstance& CI,
    llvm::StringRef File)
{
    return std::make_unique<metaRelsReader>(CI.getASTContext());
}
