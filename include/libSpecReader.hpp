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

#include "clang_interface.hpp"

class ExposedFuncDecl
{

    public:

        std::string name;
        const clang::CXXRecordDecl* enclosing_class = nullptr;
        clang::QualType ret_typ;
        llvm::ArrayRef<clang::ParmVarDecl*> params;
        bool statik;
        bool ctor;

        ExposedFuncDecl(llvm::StringRef _name, const clang::CXXRecordDecl* _ec,
            clang::QualType _ret_typ,
            llvm::ArrayRef<clang::ParmVarDecl*> _params, bool _statik = false,
            bool _ctor = false) :
            name(_name), enclosing_class(_ec), ret_typ(_ret_typ),
            params(_params), statik(_statik), ctor(_ctor) {};
        ExposedFuncDecl(llvm::StringRef _name, clang::QualType _ret_typ,
            llvm::ArrayRef<clang::ParmVarDecl*> _params, bool _statik = false,
            bool _ctor = false) :
            name(_name), ret_typ(_ret_typ), params(_params), statik(_statik),
            ctor(_ctor) {};

        std::string
        getSignature() const
        {
            std::stringstream ss;
            if (this->statik)
            {
                ss << "static ";
            }
            ss << ret_typ.getAsString() << " ";
            if (enclosing_class)
            {
                ss << enclosing_class->getQualifiedNameAsString() << "::";
            }
            ss << name << "(";
            if (!this->params.empty())
            {
                ss << std::accumulate(std::next(std::begin(this->params)),
                    std::end(this->params), this->params[0]->getOriginalType().getAsString(),
                    [](std::string acc, clang::ParmVarDecl* p)
                        {
                            return acc + ',' + p->getOriginalType().getAsString();
                        });
            }
            ss << ")";
            return ss.str();
        }

        static bool
        compare(const ExposedFuncDecl& lhs, const ExposedFuncDecl& rhs)
        {
            //return lhs.getSignature().compare(rhs.getSignature()) < 0;
            if (lhs.enclosing_class && rhs.enclosing_class)
            {
                if (int enclosing_class_name_cmp =
                    lhs.enclosing_class->getName().compare(
                        rhs.enclosing_class->getName()))
                {
                    return enclosing_class_name_cmp < 0;
                }
            }
            if (int name_cmp = lhs.name.compare(rhs.name))
            {
                return name_cmp < 0;
            }
            if (int ret_cmp = lhs.ret_typ.getAsString().compare(rhs.ret_typ.getAsString()))
            {
                return ret_cmp < 0;
            }
            size_t lhs_size = lhs.params.size(), rhs_size = rhs.params.size();
            if (lhs_size != rhs_size)
            {
                return lhs_size < rhs_size;
            }
            for (size_t param_index = 0; param_index < lhs_size; ++param_index)
            {
                std::string lhs_param_str = lhs.params[param_index]
                    ->getOriginalType().getAsString();
                std::string rhs_param_str = rhs.params[param_index]
                    ->getOriginalType().getAsString();
                if (int par_cmp = lhs_param_str.compare(rhs_param_str))
                {
                    return par_cmp < 0;
                }
            }
            return false;
        }
};

const llvm::StringRef exposingAttributeStr("expose");
const std::string exposingSpecialAttributeStr = "expose_special";
std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)>
    exposed_funcs(&ExposedFuncDecl::compare);

void
addExposedFuncs()
{
    for (ExposedFuncDecl efd : exposed_funcs)
    {
        std::vector<std::string> str_params;
        std::transform(efd.params.begin(), efd.params.end(),
            std::back_inserter(str_params), [](clang::ParmVarDecl* param)
            {
                return param->getOriginalType().getAsString();
            });
        if (efd.enclosing_class)
        {
            fuzzer::clang::addLibFunc(efd.name,
                efd.enclosing_class->getQualifiedNameAsString(),
                efd.ret_typ.getAsString(), str_params, efd.statik,
                efd.ctor);
        }
        else
        {
            fuzzer::clang::addLibFunc(efd.name, "",
                efd.ret_typ.getAsString(), str_params, efd.statik,
                efd.ctor);
        }
    }
    exposed_funcs.clear();
}

/*******************************************************************************
* FuzzHelperLog action - read information from defined helper functions
*******************************************************************************/

class fuzzHelperFuncLogger : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::FunctionDecl* FD =
                    Result.Nodes.getNodeAs<clang::FunctionDecl>("helperFunc"))
            {
                exposed_funcs.emplace(FD->getQualifiedNameAsString(),
                    FD->getReturnType(), FD->parameters(), FD->isStatic(),
                    FD->getNameAsString().find("ctor") == 0);
            }
        }
};

class fuzzHelperLogger : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        fuzzHelperFuncLogger logger;

    public:
        fuzzHelperLogger()
        {
            matcher.addMatcher(
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasAncestor(
                clang::ast_matchers::namespaceDecl(
                clang::ast_matchers::hasName(
                "fuzz::lib_helper_funcs"))))
                    .bind("helperFunc"), &logger);
        }

        void HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);
        }
};

class fuzzHelperLoggerAction : public clang::ASTFrontendAction
{
    public:
        fuzzHelperLoggerAction() {};

        bool
        BeginSourceFileAction(clang::CompilerInstance& ci) override
        {
            std::cout << "[fuzzHelperLoggerAction] Parsing input file ";
            std::cout << ci.getSourceManager().getFileEntryForID(
                ci.getSourceManager().getMainFileID())->getName().str()
                << std::endl;
            return true;
        };

        void
        EndSourceFileAction() override { addExposedFuncs(); };

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File) override
        {
            return std::make_unique<fuzzHelperLogger>();
        }
};

/*******************************************************************************
* LibSpecReader action - read information from exposed library sources
*******************************************************************************/

class exposedFuncDeclMatcher : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void
        run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::CXXConstructorDecl* CD =
                    Result.Nodes.getNodeAs<clang::CXXConstructorDecl>("exposedDecl"))
            {
                if (CD->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    //CD->dump();
                    std::string ctor_name = CD->getQualifiedNameAsString();
                    ctor_name = ctor_name.erase(ctor_name.find_last_of("::") - 1);
                    exposed_funcs.emplace(ctor_name, CD->getParent(),
                        CD->getReturnType(), CD->parameters(), CD->isStatic(),
                        true);
                }
            }
            else if (const clang::CXXMethodDecl* MD =
                    Result.Nodes.getNodeAs<clang::CXXMethodDecl>("exposedDecl"))
            {
                if (MD->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    //MD->dump();
                    exposed_funcs.emplace(MD->getNameAsString(), MD->getParent(),
                        MD->getReturnType(), MD->parameters(), MD->isStatic());
                }
            }
            else if (const clang::CXXRecordDecl* RD =
                    Result.Nodes.getNodeAs<clang::CXXRecordDecl>("exposedDecl"))
            {
                if (RD->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    fuzzer::clang::addLibType(RD->getQualifiedNameAsString());
                }
            }
            else if (const clang::EnumDecl* ED =
                    Result.Nodes.getNodeAs<clang::EnumDecl>("exposedDecl"))
            {
                if (ED->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    fuzzer::clang::addLibType(ED->getQualifiedNameAsString());
                }
            }
            else if (const clang::FunctionDecl* FD =
                    Result.Nodes.getNodeAs<clang::FunctionDecl>("exposedDecl"))
            {
                if (FD->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    FD->dump();
                    FD->getReturnType()->dump();
                    exposed_funcs.emplace(FD->getQualifiedNameAsString(),
                        FD->getReturnType(), FD->parameters(), FD->isStatic());
                }
            }
        }
};

class libSpecReader : public clang::ASTConsumer
{
    private:
        clang::ast_matchers::MatchFinder matcher;
        exposedFuncDeclMatcher printer;

    public:
        libSpecReader()
        {
            std::cout << "Start constructor" << std::endl;
            matcher.addMatcher(
                clang::ast_matchers::decl(
                clang::ast_matchers::hasAttr(
                clang::attr::Annotate))
                    .bind("exposedDecl"), &printer);
            std::cout << "End constructor" << std::endl;
        };

        void HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            std::cout << "Start match" << std::endl;
            matcher.matchAST(ctx);
            std::cout << "End match" << std::endl;
        };
};

class libSpecReaderAction : public clang::ASTFrontendAction
{
    public:
        libSpecReaderAction() {};

        bool
        BeginSourceFileAction(clang::CompilerInstance& ci) override
        {
            std::cout << "[libSpecReaderAction] Parsing input file ";
            std::cout << ci.getSourceManager().getFileEntryForID(
                ci.getSourceManager().getMainFileID())->getName().str()
                << std::endl;
            return true;
        };

        void
        EndSourceFileAction() override { addExposedFuncs(); };

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File) override
        {
            std::cout << "Start action" << std::endl;
            return std::make_unique<libSpecReader>();
        }
};

#endif // _LIB_SPEC_READER_HPP
