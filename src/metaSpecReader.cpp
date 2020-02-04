#include "metaSpecReader.hpp"

extern std::map<std::pair<REL_TYPE, std::string>, std::vector<mrInfo>> meta_rel_decls;
extern std::string meta_input_var_type;

void
metaRelsLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("metaRel");
    assert(FD);
    metaRelsReader::logMetaRelDecl(FD);
}

metaRelsReader::metaRelsReader()
{
    mr_matcher.addMatcher(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "metalib"))))
            .bind("metaRel"), &logger);
}

void
metaRelsReader::HandleTranslationUnit(clang::ASTContext& ctx)
{
    mr_matcher.matchAST(ctx);
}

void
metaRelsReader::logMetaRelDecl(const clang::FunctionDecl* fd)
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
    new_mr_decl.base_func->dump();
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
    return std::make_unique<metaRelsReader>();
}
