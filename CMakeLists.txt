cmake_minimum_required(VERSION 3.13)
project(MetaLibClangVer)
set(CMAKE_CXX_STANDARD 17)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/libs)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

include_directories("${INCLUDE_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/third_party/library-metamorphic-testing/include")
link_directories("${LIB_DIR}")

find_package(Clang REQUIRED)
find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if (NOT LLVM_ENABLE_RTTI)
    message(STATUS "LLVM compiled with RTTI disabled...")
    add_compile_options("-fno-rtti")
endif()

add_executable(mtFuzzer
    ${SRC_DIR}/fuzzMetaTest.cpp
    ${SRC_DIR}/fuzzHelperFuncStitch.cpp
    ${SRC_DIR}/parseFuzzerCalls.cpp
    ${INCLUDE_DIR}/parseFuzzSpec.hpp
    ${INCLUDE_DIR}/libSpecReader.hpp
    ${INCLUDE_DIR}/fuzzHelperFuncStitch.hpp
    ${INCLUDE_DIR}/parseFuzzerCalls.hpp
    )

#target_compile_options(mtFuzzer PUBLIC -stdlib=libc++)
#target_link_options(mtFuzzer PUBLIC -stdlib=libc++ -lc++abi)

target_link_libraries(mtFuzzer
    LLVM
    clangAST
    clangASTMatchers
    clangBasic
    clangDriver
    clangFrontend
    clangRewriteFrontend
    clangSerialization
    clangStaticAnalyzerFrontend
    clangTooling
    metalib_fuzz
    )
