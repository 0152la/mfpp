#ifndef _SRC_HELPER_FUNCTIONS_HPP
#define _SRC_HELPER_FUNCTIONS_HPP

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "globals.hpp"

#include <chrono>
#include <ctime>
#include <iostream>

namespace fuzz_helpers {

void EMIT_PASS_START_DEBUG(clang::CompilerInstance&, std::string);
void CHECK_CONDITION(bool, std::string);
std::string getMetaInputVarName(size_t);

} // namespace fuzz_helpers

#endif // _SRC_HELPER_FUNCTIONS_HPP
