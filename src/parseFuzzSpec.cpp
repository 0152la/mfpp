#include "parseFuzzSpec.hpp"

namespace fuzz_input_parse
{

//extern const std::string fuzz_config;
//extern static std::map<std::string, clang::APValue*> config_inputs;

//virtual void
//fuzzConfigRecorder::run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
//{
    //if (const clang::VarDecl* VD =
            //Result.Nodes.getNodeAs<clang::VarDecl>("inputDecl"))
    //{
        //config_inputs.insert(std::make_pair(VD->getNameAsString(), VD->evaluateValue()));
    //}
//}

//class fuzzConfigRecorder : public clang::ast_matchers::MatchFinder::MatchCallback
//{
    //public:
        //virtual void run(const clang::ast_matchers::MatchFinder::MatchResult &Result)
        //{
            //if (const clang::VarDecl* VD =
                    //Result.Nodes.getNodeAs<clang::VarDecl>("inputDecl"))
            //{
                //config_inputs.insert(std::make_pair(VD->getNameAsString(), VD->evaluateValue()));
            //}
        //}
//};
//



//class fuzzConfigParser : public clang::ASTConsumer
//{
    //private:
        //clang::ast_matchers::MatchFinder matcher;
        //fuzzConfigRecorder recorder;

    //public:
        //fuzzConfigParser()
        //{
            //matcher.addMatcher(
                //clang::ast_matchers::varDecl(
                //clang::ast_matchers::hasAncestor(
                //clang::ast_matchers::namespaceDecl(
                //clang::ast_matchers::hasName(
                //"fuzz::input"))))
                    //.bind("inputDecl"), &recorder);
        //}

        //void
        //HandleTranslationUnit(clang::ASTContext& ctx) override
        //{
            //matcher.matchAST(ctx);
        //}
//};

std::unique_ptr<clang::ASTConsumer>
parseFuzzConfigAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef file)
{
    return std::make_unique<fuzzConfigParser>();
}

//class parseFuzzConfigAction : public clang::ASTFrontendAction
//{
    //public:
        //parseFuzzConfigAction() {}

        //std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI,
            //llvm::StringRef file) override
            //{
                //return llvm::make_unique<fuzzConfigParser>(this);
            //}
//};

} // namespace fuzz_input_parse
