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

// helperFuncStitch.cpp
extern std::string output_file;

// srcHelperFunctions.cpp
extern std::chrono::time_point<std::chrono::system_clock> START_TIME;
const char suffix_delim = '_';

}

#endif // _GLOBALS_HPP
