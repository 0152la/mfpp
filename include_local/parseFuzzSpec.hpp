#ifndef _PARSE_FUZZ_SPEC_HPP
#define _PARSE_FUZZ_SPEC_HPP

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <map>
#include <iostream>

namespace fuzz_input_parse
{

const std::string fuzz_config = "./input/spec_fuzz.hpp";
static std::map<std::string, clang::APValue*> config_inputs;

class fuzzConfigRecorder : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::VarDecl* VD =
                    Result.Nodes.getNodeAs<clang::VarDecl>("inputDecl"))
            {
                config_inputs.insert(std::make_pair(VD->getNameAsString(), VD->evaluateValue()));
            }
        }
};

class fuzzConfigParser : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        fuzzConfigRecorder recorder;

    public:
        fuzzConfigParser::fuzzConfigParser()
        {
            matcher.addMatcher(
                clang::ast_matchers::varDecl(
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::namespaceDecl(
                clang::ast_matchers::hasName(
                "fuzz::input"))))
                    .bind("inputDecl"), &recorder);
        }

        void
        fuzzConfigParser::HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);
        }
};

class parseFuzzConfigAction : public clang::ASTFrontendAction
{
    public:
        parseFuzzConfigAction() {}

        std::unique_ptr<clang::ASTConsumer>
        parseFuzzConfigAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef file)
        {
            return std::make_unique<fuzzConfigParser>();
        }
};

} // namespace fuzz_input_parse

#endif // _PARSE_FUZZ_SPEC_HPP
