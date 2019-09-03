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

class ExposedFuncDecl
{

    public:

        std::string name;
        clang::QualType ret_typ;
        llvm::ArrayRef<clang::ParmVarDecl*> params;

        ExposedFuncDecl(llvm::StringRef _name, clang::QualType _ret_typ,
            llvm::ArrayRef<clang::ParmVarDecl*> _params) :
            name(_name), ret_typ(_ret_typ), params(_params) {};

        std::string
        getSignature() const
        {
            std::stringstream ss;
            ss << ret_typ.getAsString() << " " << name << "(";
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
            std::cout << "Comparing " << lhs.name << " and " << rhs.name << std::endl;
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

class exposedFuncDeclMatcher : public clang::ast_matchers::MatchFinder::MatchCallback
{
    public:
        virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
        {
            if (const clang::CXXMethodDecl* MD =
                    Result.Nodes.getNodeAs<clang::CXXMethodDecl>("annotateFnDecl"))
            {
                if (MD->getAttr<clang::AnnotateAttr>()->getAnnotation()
                        .equals(exposingAttributeStr))
                {
                    exposed_funcs.emplace(MD->getNameAsString(), MD->getReturnType(),
                        MD->parameters());
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
                clang::ast_matchers::functionDecl(
                clang::ast_matchers::hasAttr(
                clang::attr::Annotate))
                    .bind("annotateFnDecl"), &printer);
        };

        void HandleTranslationUnit(clang::ASTContext& ctx) override
        {
            matcher.matchAST(ctx);
        };
};

class libSpecReaderAction : public clang::ASTFrontendAction
{
    public:
        libSpecReaderAction() {};

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File)
        {
            return std::make_unique<libSpecReader>();
        }
};

#endif // _LIB_SPEC_READER_HPP
