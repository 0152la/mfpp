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

#ifndef META_SPEC_READER_HPP
#define META_SPEC_READER_HPP

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <sstream>

#include "helperFuncStitch.hpp"

enum REL_TYPE
{
    GENERATOR,
    RELATION,
    CHECK,
};

class mrInfo: public helperFnDeclareInfo
{
    public:
        REL_TYPE mr_type;
        bool is_base_func = true;
        std::string mr_name = "";
        std::string mr_family = "";
        //std::map<const clang::Stmt*, std::vector<const clang::CallExpr*>> recursive_calls;
        std::set<const clang::CallExpr*> recursive_calls;

        mrInfo(const clang::FunctionDecl* _fn);

        bool operator<(const mrInfo& other) const
        {
            return this->base_func->getQualifiedNameAsString() <
                other.base_func->getQualifiedNameAsString();
        };

        REL_TYPE getType() const { return this->mr_type; };
        std::string getFamily() const { return this->mr_family; };

        bool isCheck() const { return this->mr_type == CHECK; };

        static mrInfo empty() { return mrInfo(nullptr); };
};

class mrDRELogger: public clang::ast_matchers::MatchFinder::MatchCallback
{

    public:
        std::map<const clang::FunctionDecl*, std::set<const clang::DeclRefExpr*>> matched_dres;
        std::map<const clang::FunctionDecl*, std::set<const clang::VarDecl*>> matched_vds;

        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaRelsLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        std::vector<const clang::FunctionDecl*> matched_fds;

        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&) override;
};

class metaRelsReader : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder mr_matcher;
        clang::ast_matchers::MatchFinder mr_dre_matcher;
        metaRelsLogger mr_logger;
        mrDRELogger dre_logger;
        clang::ASTContext& ctx;

    public:
        metaRelsReader(clang::ASTContext& _ctx);

        void HandleTranslationUnit(clang::ASTContext&) override;
        void logMetaRelDecl(const clang::FunctionDecl*);
};

class metaRelsReaderAction : public clang::ASTFrontendAction
{
    public:
        metaRelsReaderAction() {};

        bool BeginSourceFileAction(clang::CompilerInstance&) override;

        std::unique_ptr<clang::ASTConsumer>
            CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef)
            override;
};


#endif // META_SPEC_READER_HPP
