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
void CHECK_CONDITION(bool, std::string);
std::string getMetaInputVarName(size_t);
std::string getMetaVarName(size_t);
std::string getBuiltinRandStr(const clang::BuiltinType*);

} // namespace fuzz_helpers

#endif // _SRC_HELPER_FUNCTIONS_HPP
