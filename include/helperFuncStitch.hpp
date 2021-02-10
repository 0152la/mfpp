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

#ifndef HELPER_FUNC_STITCH_HPP
#define HELPER_FUNC_STITCH_HPP

#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <iostream>
#include <numeric>

#include "globals.hpp"
#include "srcHelperFunctions.hpp"

typedef std::pair<std::vector<clang::Stmt*>, clang::Stmt*> helpFnSplit;

const clang::ast_type_traits::DynTypedNode
getBaseParent(const clang::ast_type_traits::DynTypedNode, clang::ASTContext&);
const clang::Stmt*
getBaseParent(const clang::Expr* e, clang::ASTContext& ctx);

class helperFnDeclareInfo
{
    public:
        const clang::FunctionDecl* base_func = nullptr;
        std::vector<clang::Stmt*> body_instrs;
        clang::Stmt* return_body = nullptr;
        std::set<const clang::DeclRefExpr*> body_dre;
        std::set<const clang::VarDecl*> body_vd;

    helperFnDeclareInfo(const clang::FunctionDecl* _fn) : base_func(_fn) {};

    std::pair<std::string, std::string>
        getSplitWithReplacements(
            std::map<const clang::ParmVarDecl*, const clang::Stmt*>,
            clang::Rewriter&, size_t);

    bool is_empty() { return this->base_func == nullptr; };
};

class helperFnReplaceInfo
{
    public:
        size_t index;
        const clang::CallExpr* call_expr;
        const clang::Stmt* base_stmt;
        std::map<const clang::ParmVarDecl*, const clang::Stmt*>
            concrete_params;

        helperFnReplaceInfo(const clang::CallExpr*, const clang::Stmt*);

};

class fuzzHelperFuncReplacer
{
    private:
        clang::ASTContext& ctx;
        clang::Rewriter& rw;

    public:
        fuzzHelperFuncReplacer(clang::ASTContext& _ctx, clang::Rewriter& _rw) :
            ctx(_ctx), rw(_rw) {}

        void makeReplace(std::vector<helperFnReplaceInfo>&) const;
};

class fuzzHelperMainLocator : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class fuzzHelperFuncLocator : public clang::ast_matchers::MatchFinder::MatchCallback
{
    private:
        clang::ASTContext& ctx;

    public:
        fuzzHelperFuncLocator(clang::ASTContext& _ctx) : ctx(_ctx) {};

        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

/** @brief Replaces helper function calls in test body with the function body
 *
 * In order to allow for comprehension usage in helper functions and do allow
 * more options for randomness, we replace helper function calls with the
 * respective body. This allows for comprehensions used to have different actual
 * values across invocations.
 */
class fuzzHelperFuncStitch : public clang::ASTConsumer
{
    private:
	fuzzHelperMainLocator main_locator;
        fuzzHelperFuncLocator locator;
        fuzzHelperFuncReplacer replacer;

        clang::ast_matchers::MatchFinder matcher;
        clang::ast_matchers::MatchFinder main_matcher;
        clang::Rewriter& rw;

    public:
        fuzzHelperFuncStitch(clang::Rewriter&, clang::ASTContext&);
        void HandleTranslationUnit(clang::ASTContext&);
};

/** @brief Action wrapper for fuzzHelperFuncStitch
 *
 */
class fuzzHelperFuncStitchAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;

    public:
        fuzzHelperFuncStitchAction() {}
        bool BeginSourceFileAction(clang::CompilerInstance&) override;
        void EndSourceFileAction() override;
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
            clang::CompilerInstance&, llvm::StringRef) override;
};

#endif // HELPER_FUNC_STITCH_HPP
