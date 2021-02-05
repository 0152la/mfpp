if [ $? -ne 5 ]
then
    echo "Expected five arguments"
    exit 1
fi

spec_ast_dir=$1
spec_ast_yaml=$2
lib_build_dir=$3
gcov_prefix=$4
time=$5

export GCOV_PREFIX=$gcov_prefix

curr_dir=`pwd`
cd $spec_ast_dir
./scripts/run_experiments.py --append-id --mode time --mode-val $time $spec_ast_yaml
cd $gcov_prefix/./$lib_build_dir
gcovr --object-directory $lib_build_dir
