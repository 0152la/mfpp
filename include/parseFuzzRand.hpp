 /******************************************************************************
 *  Copyright 2021 Andrei Lascu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#ifndef _PARSE_FUZZ_RAND_HPP
#define _PARSE_FUZZ_RAND_HPP

#include "clang_interface.hpp"

class fuzzerCallsReplacer
{
    private:
        clang::ASTContext& ctx;
        clang::Rewriter& rw;

    public:
        fuzzerCallsReplacer(clang::ASTContext& _ctx, clang::Rewriter& _rw) :
            ctx(_ctx), rw(_rw) {}

        //void makeReplace(std::vector<helperFnReplaceInfo>&) const;
};

class fuzzerCallsLocator: public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::ASTContext& ctx;

    public:
        fuzzerCallsLocator(clang::ASTContext& _ctx) : ctx(_ctx) {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class parseFuzzerCalls: public clang::ASTConsumer
{
    private:
        fuzzerCallsLocator locator;
        fuzzerCallsReplacer replacer;

        clang::ast_matchers::MatchFinder matcher;
        clang::Rewriter& rw;

    public:
        parseFuzzerCalls(clang::Rewriter&, clang::ASTContext&);
        void HandleTranslationUnit(clang::ASTContext&);
};

class parseFuzzerCallsAction : public clang::ASTFrontendAction
{
    public:
        parseFuzzerCallsAction() {}

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef) override;
};

#endif // _PARSE_FUZZ_RAND_HPP
