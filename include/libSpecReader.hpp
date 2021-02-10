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

#ifndef _LIB_SPEC_READER_HPP
#define _LIB_SPEC_READER_HPP

#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

#include <numeric>
#include <iostream>
#include <sstream>

#include "srcHelperFunctions.hpp"
#include "clang_interface.hpp"

void addExposedFuncs(const clang::PrintingPolicy&);

class ExposedFuncDecl
{

    public:

        std::string name;
        const clang::CXXRecordDecl* enclosing_class = nullptr;
        clang::QualType ret_typ;
        llvm::ArrayRef<clang::ParmVarDecl*> params;
        bool statik;
        bool ctor;
        bool special;

        ExposedFuncDecl(llvm::StringRef _name, const clang::CXXRecordDecl* _ec,
            clang::QualType _ret_typ,
            llvm::ArrayRef<clang::ParmVarDecl*> _params, bool _statik = false,
            bool _ctor = false, bool _special = false) :
            name(_name), enclosing_class(_ec), ret_typ(_ret_typ),
            params(_params), statik(_statik), ctor(_ctor), special(_special) {};
        ExposedFuncDecl(llvm::StringRef _name, clang::QualType _ret_typ,
            llvm::ArrayRef<clang::ParmVarDecl*> _params, bool _statik = false,
            bool _ctor = false, bool _special = false) :
            name(_name), ret_typ(_ret_typ), params(_params), statik(_statik),
            ctor(_ctor), special(_special) {};

        std::string getSignature() const;
        static bool compare(const ExposedFuncDecl&, const ExposedFuncDecl&);
};

class ExposedTemplateType
{
    public:
        std::string base_type;
        clang::TemplateParameterList* template_params;

        ExposedTemplateType(std::string _base_type, clang::TemplateParameterList* _tpl) :
            base_type(_base_type), template_params(_tpl) {};

        std::vector<std::string> getParamListStr();
        static bool compare(const ExposedTemplateType&, const ExposedTemplateType&);
};

/*******************************************************************************
* FuzzHelperLog action - read information from defined helper functions
*******************************************************************************/

class fuzzHelperFuncLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class fuzzHelperLogger : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        fuzzHelperFuncLogger logger;

    public:
        fuzzHelperLogger();

        void HandleTranslationUnit(clang::ASTContext& ctx) override;
};

class fuzzHelperLoggerAction : public clang::ASTFrontendAction
{
    private:
        const clang::PrintingPolicy* print_policy;

    public:
        fuzzHelperLoggerAction() {};

        bool BeginSourceFileAction(clang::CompilerInstance& ci) override;
        void EndSourceFileAction() override;

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File) override;
};

/*******************************************************************************
* LibSpecReader action - read information from exposed library sources
*******************************************************************************/

class exposedFuncDeclMatcher : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult&);
};

class libSpecReader : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        exposedFuncDeclMatcher printer;

    public:
        libSpecReader();

        void HandleTranslationUnit(clang::ASTContext& ctx) override;
};

class libSpecReaderAction : public clang::ASTFrontendAction
{
    private:
        const clang::PrintingPolicy* print_policy;

    public:
        libSpecReaderAction() {};

        bool BeginSourceFileAction(clang::CompilerInstance& ci) override;
        void EndSourceFileAction() override;

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File) override;
};

extern std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)>
    exposed_funcs;

#endif // _LIB_SPEC_READER_HPP
