cmake_minimum_required(VERSION 3.14)
project(MetaLibClangVer)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER clang++)
#set(CMAKE_CXX_COMPILER g++)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/libs)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories("${INCLUDE_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/include_local")
include_directories("${CMAKE_SOURCE_DIR}/include_clang")
include_directories("${CMAKE_SOURCE_DIR}/include_libfuzz")
#include_directories("/home/sentenced/Documents/Internships/2018_ETH/work/testing_functional/include")
link_directories("${LIB_DIR}")
link_directories("/usr/local/lib/")

find_package(Clang REQUIRED)
find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if (NOT LLVM_ENABLE_RTTI)
    message(STATUS "LLVM compiled with RTTI disabled...")
    set(CMAKE_CXX_FLAGS "-fno-rtti")
endif()

#message(STATUS "${LLVM_DIR}")
#list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
#include(AddLLVM)
#add_llvm_library(pfn MODULE ${SRC_DIR}/PrintFunctionNames.cpp PLUGIN_TOOL clang)

#add_llvm_executable(mrParser ${SRC_DIR}/parseMetaRels.cpp)
add_executable(mrParser ${SRC_DIR}/parseMetaRels.cpp)
#llvm_config(mrParser core)
add_executable(mtFuzzer
    ${SRC_DIR}/fuzzMetaTest.cpp
    ${SRC_DIR}/fuzzHelperFuncStitch.cpp
    ${INCLUDE_DIR}/parseFuzzSpec.hpp
    ${INCLUDE_DIR}/libSpecReader.hpp
    ${INCLUDE_DIR}/fuzzHelperFuncStitch.hpp
    )

target_link_libraries(mrParser PRIVATE
    LLVM
    clangAST
    clangASTMatchers
    clangBasic
    clangDriver
    clangFrontend
    #clangSerialization
    clangTooling
    fmt
    yaml-cpp
    metalib_fuzz
    )

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
    fmt
    yaml-cpp
    metalib_fuzz
    )
