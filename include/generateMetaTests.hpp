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

#ifndef GENERATE_META_TESTS_HPP
#define GENERATE_META_TESTS_HPP

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <sstream>

#include "metaSpecReader.hpp"
#include "clang_interface.hpp"
#include "srcHelperFunctions.hpp"

struct mrGenInfo
{
    mrInfo* mr_decl;
    std::string curr_mr_var_name;
    std::vector<std::string> input_var_names;
    size_t depth = 0, test_idx, recursive_idx = 0, family_idx = 0;
    bool first_decl = true;

    mrGenInfo(mrInfo* _mr_decl, std::string _curr_mr_var_name,
        std::vector<std::string> _input_var_names, size_t _test_idx) :
        mr_decl(_mr_decl), curr_mr_var_name(_curr_mr_var_name),
        input_var_names(_input_var_names), test_idx(_test_idx) {};

    mrGenInfo(std::string _curr_mr_var_name,
        std::vector<std::string> _input_var_names, size_t _test_idx) :
        mrGenInfo(nullptr, _curr_mr_var_name, _input_var_names, _test_idx) {};

    void setMR(mrInfo*);
};

std::string retrieveMRDeclVar(mrInfo*, const clang::Type*);
mrInfo retrieveRandMrDecl(std::string, std::string, bool = false);
mrInfo retrieveRandMrDecl(REL_TYPE, std::string, bool = false);


class testMainLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaCallsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class mrRecursiveLogger: public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        std::map<const clang::FunctionDecl*, std::set<const clang::CallExpr*>> matched_recursive_calls;
        //std::map<const clang::FunctionDecl*,
            //std::map<const clang::Stmt*, std::vector<const clang::CallExpr*>>>
            //matched_recursive_calls;

        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class MRTraverser : public clang::RecursiveASTVisitor<MRTraverser>
{
    private:
        mrInfo& mri;

    public:
        MRTraverser(mrInfo& _mri) : mri(_mri)
            { this->TraverseDecl(const_cast<clang::FunctionDecl*>(this->mri.base_func)); };

        bool TraverseDecl(clang::Decl*);
        bool TraverseStmt(clang::Stmt*);
        bool TraverseType(clang::QualType);

        bool VisitCallExpr(clang::CallExpr*);
        bool VisitVarDecl(clang::VarDecl*);
        bool VisitDeclRefExpr(clang::DeclRefExpr*);

};

class metaGenerator : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        testMainLogger main_logger;
        metaCallsLogger mc_logger;
        metaRelsLogger mr_logger;
        mrDRELogger mr_dre_logger;
        mrRecursiveLogger mr_recursive_logger;
        clang::Rewriter& rw;
        clang::ASTContext& ctx;

        const std::string recursive_func_call_name = "placeholder";
        const std::string mr_namespace_name = "metalib";
        std::string indent = "";

    public:
        metaGenerator(clang::Rewriter&, clang::ASTContext&);

        void HandleTranslationUnit(clang::ASTContext&) override;
        void logMetaRelDecl(const clang::FunctionDecl*);
        void expandMetaTests();

        std::string generateMetaTests(std::vector<std::string>);
        std::string generateSingleMetaTest(std::vector<std::string>,
            const std::vector<std::string>&, size_t);

        std::pair<std::string, std::string> concretizeMetaRelation(mrGenInfo&);
        std::string makeUniqueFuncCallName(mrGenInfo&);
        std::string makeMRFuncCall(mrGenInfo&, mrInfo* = nullptr,
            std::vector<std::string> = std::vector<std::string>(), bool = false);
        void makeRecursiveFunctionCalls(mrGenInfo&, std::stringstream&);
};

class metaGeneratorAction : public clang::ASTFrontendAction
{
    private:
        clang::Rewriter rw;

    public:
        metaGeneratorAction() {};

        bool BeginSourceFileAction(clang::CompilerInstance&) override;
        void EndSourceFileAction() override;

        std::unique_ptr<clang::ASTConsumer>
            CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef)
            override;
};

#endif // GENERATE_META_TESTS_HPP
