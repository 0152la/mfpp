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

class ExposedTemplateType
{
    public:
        std::string base_type;
        clang::TemplateParameterList* template_params;

        ExposedTemplateType(std::string _base_type, clang::TemplateParameterList* _tpl) :
            base_type(_base_type), template_params(_tpl) {};

        std::vector<std::string>
        getParamListStr()
        {
            std::vector<std::string> template_params_str;
            std::transform(template_params->begin(), template_params->end(),
                std::back_inserter(template_params_str),
                [](clang::NamedDecl* ND)
                {
                    return ND->getQualifiedNameAsString();
                });
            return template_params_str;
        }

        static bool
        compare(const ExposedTemplateType& lhs, const ExposedTemplateType& rhs)
        {
            if (int base_type_cmp = (lhs.base_type.compare(rhs.base_type)))
            {
                return base_type_cmp < 0;
            }
            if (lhs.template_params->size() != rhs.template_params->size())
            {
                return lhs.template_params->size() < rhs.template_params->size();
            }
            for (size_t i = 0; i < lhs.template_params->size(); ++i)
            {
                if (int param_cmp =
                        (lhs.template_params->getParam(i) !=
                            rhs.template_params->getParam(i)))
                {
                    return param_cmp < 0;
                }
            }
            return false;
        }
};

const llvm::StringRef exposingAttributeStr("expose");
const std::string exposingSpecialAttributeStr = "expose_special";
std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)>
    exposed_funcs(&ExposedFuncDecl::compare);
std::set<ExposedTemplateType, decltype(&ExposedTemplateType::compare)>
    exposed_template_types(&ExposedTemplateType::compare);

void
addExposedFuncs(const clang::PrintingPolicy& print_policy)
{
    for (ExposedFuncDecl efd : exposed_funcs)
    {
        std::vector<std::string> str_params;
        std::transform(efd.params.begin(), efd.params.end(),
            std::back_inserter(str_params), [](clang::ParmVarDecl* param)
            {
                return param->getOriginalType().getAsString();
            });
        const clang::BuiltinType* BT =
                llvm::dyn_cast<clang::BuiltinType>(efd.ret_typ);
        std::string return_type_str = BT
            ? BT->getName(print_policy).str()
            : efd.ret_typ.getAsString();
        std::string enclosing_class_str = efd.enclosing_class
            ? efd.enclosing_class->getQualifiedNameAsString()
            : "";

        fuzzer::clang::addLibFunc(efd.name, enclosing_class_str,
            return_type_str, str_params, efd.statik, efd.ctor);
        //if (efd.enclosing_class)
        //{
            //fuzzer::clang::addLibFunc(efd.name,
                //efd.enclosing_class->getQualifiedNameAsString(),
                //efd.ret_typ.getAsString(), str_params, efd.statik,
                //efd.ctor);
        //}
        //else
        //{
            //fuzzer::clang::addLibFunc(efd.name, "",
                //efd.ret_typ.getAsString(), str_params, efd.statik,
                //efd.ctor);
        //}
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
    private:
        const clang::PrintingPolicy* print_policy;

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
        EndSourceFileAction() override { addExposedFuncs(*this->print_policy); };

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File) override
        {
            assert(CI.hasASTContext());
            this->print_policy = &CI.getASTContext().getPrintingPolicy();
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
                    //MD->getReturnType().dump();
                    //if (const clang::BuiltinType* BT = llvm::dyn_cast<clang::BuiltinType>(MD->getReturnType()))
                    //{
                        //BT->desugar().dump();
                        //std::cout << BT->getName(this->PP).str() << '\n';
                        //std::cout << BT->getNameAsCString(this->PP) << '\n';
                    //}
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
                    //FD->dump();
                    //FD->getReturnType()->dump();
                    exposed_funcs.emplace(FD->getQualifiedNameAsString(),
                        FD->getReturnType(), FD->parameters(), FD->isStatic());
                }
            }
            else if (const clang::TypeAliasDecl* TAD =
                    Result.Nodes.getNodeAs<clang::TypeAliasDecl>("exposedDecl"))
            {
                if (TAD->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    //TAD->dump();
                    //TAD->getDescribedAliasTemplate()->dump();
                    if (clang::TypeAliasTemplateDecl* TATD =
                            TAD->getDescribedAliasTemplate())
                    {
                        fuzzer::clang::addLibTemplateType(
                            TAD->getQualifiedNameAsString(),
                            TATD->getTemplateParameters()->size());
                        //exposed_template_types.emplace(
                            //TAD->getQualifiedNameAsString(),
                            //TATD->getTemplateParameters()->size());

                        //std::vector<std::string> template_str_list;
                        //std::transform(TATD->getTemplateParameters()->begin(),
                            //TATD->getTemplateParameters()->end(),
                            //std::back_inserter(template_str_list),
                            //[](clang::NamedDecl* ND)
                            //{
                                //return ND->getQualifiedNameAsString();
                            //});
                        //fuzzer::clang::addLibExposedTemplateType(
                            //TAD->getQualifiedNameAsString(),
                            //template_str_list);
                    }
                    else
                    {
                        fuzzer::clang::addLibType(TAD->getQualifiedNameAsString());
                    }
                }
            }
            else if (const clang::TypedefDecl* TDD =
                    Result.Nodes.getNodeAs<clang::TypedefDecl>("exposedDecl"))
            {
                if (TDD->getUnderlyingType().getAsString().back() == '*')
                {
                    fuzzer::clang::addLibType(TDD->getQualifiedNameAsString(),
                        true, false);
                }
                else
                {
                    fuzzer::clang::addLibType(TDD->getQualifiedNameAsString());
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
            matcher.addMatcher(
                clang::ast_matchers::decl(
                clang::ast_matchers::hasAttr(
                clang::attr::Annotate))
                    .bind("exposedDecl"), &printer);
        };

        void HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);
        };
};

class libSpecReaderAction : public clang::ASTFrontendAction
{
    private:
        const clang::PrintingPolicy* print_policy;

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
        EndSourceFileAction() override { addExposedFuncs(*this->print_policy); };

        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File) override
        {
            assert(CI.hasASTContext());
            this->print_policy = &CI.getASTContext().getPrintingPolicy();
            return std::make_unique<libSpecReader>();
        }
};

#endif // _LIB_SPEC_READER_HPP
