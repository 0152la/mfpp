#ifndef _GLOBALS_HPP
#define _GLOBALS_HPP

#include "clang/AST/Type.h"

#include <set>

// generateMetaTests.cpp
// parseFuzzSpec.cpp
extern std::string meta_var_name;
extern size_t meta_input_fuzz_count;
extern size_t meta_test_rel_count;
extern size_t meta_test_count;
extern size_t meta_test_depth;
extern std::string meta_input_var_prefix;

// generateMetaTests.cpp
// parseFuzzSpec.cpp
extern llvm::SmallString<256> rewritten_input_file;
extern std::string rewrite_data;
extern std::string set_meta_tests_path;

// helperFuncStitch.cpp
extern std::string output_file;

// srcHelperFunctions.cpp
extern std::chrono::time_point<std::chrono::system_clock> START_TIME;

#endif // _GLOBALS_HPP
