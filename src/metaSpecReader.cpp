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

#include "metaSpecReader.hpp"

void
mrDRELogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("mrFuncDecl");
    assert(FD);
    if (!this->matched_vds.count(FD))
    {
        this->matched_vds.emplace(std::make_pair(FD, std::set<const clang::VarDecl*>()));
        this->matched_dres.emplace(std::make_pair(FD, std::set<const clang::DeclRefExpr*>()));
    }
    if (const clang::VarDecl* VD = Result.Nodes.getNodeAs<clang::VarDecl>("mrVD"))
    {
        //std::cout << "VD DUMP" << std::endl;
        //VD->dump();
        this->matched_vds.at(FD).insert(VD);
        return;
    }
    else if (const clang::DeclRefExpr* DRE =
            Result.Nodes.getNodeAs<clang::DeclRefExpr>("mrDRE"))
    {
        if (llvm::dyn_cast<clang::ParmVarDecl>(DRE->getDecl()) ||
                llvm::dyn_cast<clang::VarDecl>(DRE->getDecl()))
        {
            //std::cout << "DRE DUMP" << std::endl;
            //DRE->dump();
            this->matched_dres.at(FD).insert(DRE);
        }
        return;
    }
    assert(false);
}

void
metaRelsLogger::run(const clang::ast_matchers::MatchFinder::MatchResult& Result)
{
    const clang::FunctionDecl* FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("metaRel");
    assert(FD);
    this->matched_fds.push_back(FD);
}
