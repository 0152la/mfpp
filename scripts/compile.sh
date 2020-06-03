#!/bin/bash
if [[ "$#" -ne 2 ]]
then
    echo "Expected arguments: <test_name> <cmake_script_location>"
fi
test_name=$1
cmake_loc=$2
build_dir=`mktemp -d`
echo "Compiling $1 with script at $2"
cd $build_dir
cmake -G Ninja -DTEST_FILE=$test_name $2 && ninja
mv `basename ${test_name%.*}` `dirname $test_name`
cd - && rm -rf $build_dir

