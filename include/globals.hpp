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

#ifndef _GLOBALS_HPP
#define _GLOBALS_HPP

#include "clang/AST/Type.h"

#include <set>

namespace globals
{

// generateMetaTests.cpp
// parseFuzzSpec.cpp
// fuzzMetaTest.cpp
const std::string meta_var_name = "r";
extern size_t meta_input_fuzz_count;
extern size_t meta_test_rel_count;
extern size_t meta_test_count;
extern size_t meta_test_depth;
const std::string meta_input_var_prefix = "output_var";
const std::string meta_input_var_get_prefix = "output_var_get";

// generateMetaTests.cpp
// parseFuzzSpec.cpp
extern llvm::SmallString<256> rewritten_input_file;
extern std::string rewrite_data;

// generateMetaTests.cpp
// fuzzMetaTest.cpp
extern bool trivial_check;

// helperFuncStitch.cpp
extern std::string output_file;

// srcHelperFunctions.cpp
extern std::chrono::time_point<std::chrono::system_clock> START_TIME;
const char suffix_delim = '_';

}

#endif // _GLOBALS_HPP
