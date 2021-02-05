if [ $# -ne 5 ]
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

# Execute experiments
curr_dir=`pwd`
cd $spec_ast_dir
./scripts/run_experiments.py --append-id --debug --mode time --mode-val $time $spec_ast_yaml

# Gather gcno files
cd $lib_build_dir
gcno_archive_name=$(mktemp -u lib_gcno_XXX.tar)
find . -name "*gcno" -exec tar -rf $gcno_archive_name {} \;

# Move gcno files and gather coverage
cd $gcov_prefix.$lib_build_dir
mv $lib_build_dir/$gcno_archive_name .
tar -xf $gcno_archive_name
gcovr --object-directory $lib_build_dir
