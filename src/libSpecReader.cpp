#include "libSpecReader.hpp"

std::set<ExposedFuncDecl, decltype(&ExposedFuncDecl::compare)>
    exposed_funcs(&ExposedFuncDecl::compare);
const llvm::StringRef exposingAttributeStr("expose");
const llvm::StringRef exposingSpecialAttributeStr("expose_special");
std::set<ExposedTemplateType, decltype(&ExposedTemplateType::compare)>
    exposed_template_types(&ExposedTemplateType::compare);

std::string
ExposedFuncDecl::getSignature() const
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

bool
ExposedFuncDecl::compare(const ExposedFuncDecl& lhs, const ExposedFuncDecl& rhs)
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

std::vector<std::string>
ExposedTemplateType::getParamListStr()
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

bool
ExposedTemplateType::compare(const ExposedTemplateType& lhs,
    const ExposedTemplateType& rhs)
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
            return_type_str, str_params, efd.statik, efd.ctor, efd.special);
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

void
fuzzHelperFuncLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    if (const clang::FunctionDecl* FD =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("helperFunc"))
    {
        exposed_funcs.emplace(FD->getQualifiedNameAsString(),
            FD->getReturnType(), FD->parameters(), FD->isStatic(),
            FD->getNameAsString().find("ctor") == 0, false);
        return;
    }

    if (const clang::TypedefDecl* TDD =
            Result.Nodes.getNodeAs<clang::TypedefDecl>("helperTypedef"))
    {
        fuzzer::clang::addLibType(TDD->getQualifiedNameAsString());
        return;
    }

    assert(false);
}

fuzzHelperLogger::fuzzHelperLogger()
{
    matcher.addMatcher(
        clang::ast_matchers::functionDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "fuzz::lib_helper_funcs"))))
            .bind("helperFunc"), &logger);

    matcher.addMatcher(
        clang::ast_matchers::typedefDecl(
        clang::ast_matchers::hasAncestor(
        clang::ast_matchers::namespaceDecl(
        clang::ast_matchers::hasName(
        "fuzz"))))
            .bind("helperTypedef"), &logger);
}

void
exposedFuncDeclMatcher::run(
    const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::Decl* D = Result.Nodes.getNodeAs<clang::Decl>("exposedDecl");
    llvm::StringRef attr = D->getAttr<clang::AnnotateAttr>()->getAnnotation();
    std::vector<llvm::StringRef> exposingAttrStrVec =
        { exposingAttributeStr, exposingSpecialAttributeStr };
    assert(std::find(std::begin(exposingAttrStrVec),
        std::end(exposingAttrStrVec), attr) != std::end(exposingAttrStrVec));
    bool expose_special = attr.equals(exposingSpecialAttributeStr);
    if (const clang::CXXConstructorDecl* CD =
            Result.Nodes.getNodeAs<clang::CXXConstructorDecl>("exposedDecl"))
    {
        //CD->dump();
        std::string ctor_name = CD->getQualifiedNameAsString();
        ctor_name = ctor_name.erase(ctor_name.find_last_of("::") - 1);
        exposed_funcs.emplace(ctor_name, CD->getParent(),
            CD->getReturnType(), CD->parameters(), CD->isStatic(),
            true, expose_special);
    }
    else if (const clang::CXXMethodDecl* MD =
            Result.Nodes.getNodeAs<clang::CXXMethodDecl>("exposedDecl"))
    {
        exposed_funcs.emplace(MD->getNameAsString(), MD->getParent(),
            MD->getReturnType(), MD->parameters(), MD->isStatic(),
            false, expose_special);
    }
    else if (const clang::CXXRecordDecl* RD =
            Result.Nodes.getNodeAs<clang::CXXRecordDecl>("exposedDecl"))
    {
        assert(!expose_special);
        fuzzer::clang::addLibType(RD->getQualifiedNameAsString());
    }
    else if (const clang::EnumDecl* ED =
            Result.Nodes.getNodeAs<clang::EnumDecl>("exposedDecl"))
    {
        assert(!expose_special);
        fuzzer::clang::addLibEnumType(ED->getQualifiedNameAsString());
    }
    else if (const clang::FunctionDecl* FD =
            Result.Nodes.getNodeAs<clang::FunctionDecl>("exposedDecl"))
    {
        //FD->dump();
        //FD->getReturnType()->dump();
        exposed_funcs.emplace(FD->getQualifiedNameAsString(),
            FD->getReturnType(), FD->parameters(), FD->isStatic(),
            false, expose_special);
    }
    else if (const clang::TypeAliasDecl* TAD =
            Result.Nodes.getNodeAs<clang::TypeAliasDecl>("exposedDecl"))
    {
        assert(!expose_special);
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
    else if (const clang::TypedefDecl* TDD =
            Result.Nodes.getNodeAs<clang::TypedefDecl>("exposedDecl"))
    {
        assert(!expose_special);
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
    else if (const clang::EnumConstantDecl* ECD =
            Result.Nodes.getNodeAs<clang::EnumConstantDecl>("exposedDecl"))
    {
        assert(!expose_special);
        fuzzer::clang::addLibEnumVal(ECD->getNameAsString(), ECD->getType().getAsString());
    }
}

libSpecReader::libSpecReader()
{
    matcher.addMatcher(
        clang::ast_matchers::decl(
        clang::ast_matchers::hasAttr(
        clang::attr::Annotate))
            .bind("exposedDecl"), &printer);
}

void
fuzzHelperLogger::HandleTranslationUnit(clang::ASTContext& ctx)
{
    matcher.matchAST(ctx);
}


bool
fuzzHelperLoggerAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "fuzzHelperLoggerAction");
    return true;
};

void
fuzzHelperLoggerAction::EndSourceFileAction()
{
    addExposedFuncs(*this->print_policy);
};

std::unique_ptr<clang::ASTConsumer>
fuzzHelperLoggerAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File)
{
    this->print_policy = &CI.getASTContext().getPrintingPolicy();
    return std::make_unique<fuzzHelperLogger>();
}

void
libSpecReader::HandleTranslationUnit(clang::ASTContext& ctx)
{
    matcher.matchAST(ctx);
};

bool
libSpecReaderAction::BeginSourceFileAction(clang::CompilerInstance& ci)
{
    fuzz_helpers::EMIT_PASS_START_DEBUG(ci, "libSpecReaderAction");
    return true;
};

void
libSpecReaderAction::EndSourceFileAction()
{
    addExposedFuncs(*this->print_policy);
};

std::unique_ptr<clang::ASTConsumer>
libSpecReaderAction::CreateASTConsumer(clang::CompilerInstance& CI, llvm::StringRef File)
{
    this->print_policy = &CI.getASTContext().getPrintingPolicy();
    return std::make_unique<libSpecReader>();
}
