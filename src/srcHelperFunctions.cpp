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

#include "srcHelperFunctions.hpp"

namespace fuzz_helpers {

void
CHECK_CONDITION(bool condition, std::string msg)
{
    if (!condition)
    {
        std::cout << "ASSERTION FAILED: " << msg << std::endl;
        exit(1);
    }
}

void
EMIT_PASS_START_DEBUG(clang::CompilerInstance& ci, std::string pass_name)
{
    std::chrono::duration<double> from_start = std::chrono::system_clock::now() - globals::START_TIME;
    //std::string from_start_str(from_start.count());
    //from_start_str = from_start_str.substr(0, from_start_str.length());
    //from_start_str = "\033[1m\033[31m" + from_start_str + "\033[m";
    std::cout << "[" << "\033[1m\033[31m" << from_start.count() << "\033[m" << "]";
    std::cout << "[" << pass_name << "] Parsing input file ";
    std::cout << ci.getSourceManager().getFileEntryForID(
        ci.getSourceManager().getMainFileID())->getName().str()
        << std::endl;
}

std::string
getMetaInputVarName(size_t id)
{
    return globals::meta_input_var_prefix + globals::suffix_delim + std::to_string(id);
}

std::string
getMetaVarName(size_t id)
{
    return globals::meta_var_name + globals::suffix_delim + std::to_string(id);
}

std::string
getBuiltinRandStr(const clang::BuiltinType* bt)
{
    if (bt->isUnsignedInteger())
    {
        int min = 0, max = 20;
        return std::to_string(fuzzer::clang::generateRand(min, max));
    }
    if (bt->isInteger())
    {
        int min = -20, max = 20;
        return std::to_string(fuzzer::clang::generateRand(min, max));
    }
    if (bt->isFloatingPoint())
    {
        float min = -20.0, max = 20.0;
        return std::to_string(fuzzer::clang::generateRand(min, max));
    }
    assert(false);
}

} // namespace fuzz_helpers

//void
//EMIT_PASS_DEBUG(const std::string& pass_name, const clang::Rewriter& pass_rw)
//{
    //std::error_code ec;
    //int fd;
    //llvm::sys::fs::createTemporaryFile("", ".cpp", fd, rewritten_input_file);
    //llvm::raw_fd_ostream rif_rfo(fd, true);
    //pass_rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(rif_rfo);
//}

