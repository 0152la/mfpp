#!/bin/bash

#set -x

if [[ "$1" = "--help" ]]
then
    echo -e "Compilation script for Metalib generated test cases\n\n"
    echo -e "Usage: compile.sh <test_name> <cmake_script_location>"
    echo -e "\t<test_name> -- path to input generated test file"
    echo -e "\t<cmake_script_location> -- path to folder containing CMake script"
    echo -e "\t\t used to compile a program for the given library"
fi

if [[ "$#" -ne 2 ]]
then
    echo "Expected arguments: <test_name> <cmake_script_location>"
    exit 1
fi

test_name=$1
cmake_loc=$2
build_dir=`mktemp -d`
echo "Compiling $1 with script at $2"
cd $build_dir
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTEST_FILE=$test_name $2 && ninja
mv `basename ${test_name%.*}` `dirname $test_name`
cd - && rm -rf $build_dir

