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

#ifndef _SRC_HELPER_FUNCTIONS_HPP
#define _SRC_HELPER_FUNCTIONS_HPP

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "globals.hpp"
#include "clang_interface.hpp"

#include <chrono>
#include <ctime>
#include <iostream>

namespace fuzz_helpers {

void EMIT_PASS_START_DEBUG(clang::CompilerInstance&, std::string);
void END_PASS_WRITE_TEMP(clang::Rewriter&);
void CHECK_CONDITION(bool, std::string);
std::string getMetaInputVarName(size_t);
std::string getMetaVarName(size_t);
std::string getBuiltinRandStr(const clang::BuiltinType*);

} // namespace fuzz_helpers

#endif // _SRC_HELPER_FUNCTIONS_HPP
