### Dependencies:

* llvm/clang >=9.0
* CMake >=3.13
* git
* Ninja (**optional**)

### Building

#### Getting and building llvm from source

Binaries can be obtained either from the [official LLVM website](http://releases.llvm.org/download.html), from the user's package manager, or built from source. In case a source build is preferred the following commands are recommended:

```
# Get only llvm-9.0 files
git clone -b llvmorg-9.0.0 --single-branch https://github.com/llvm/llvm-project
# Setup local directory hierarch
mkdir llvm-build llvm-install && cd llvm-build
# Configure
cmake -G "Ninja"
-DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra;libcxx;libcxxabi' -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVM_BUILD_LLVM_DYLIB=ON -DCMAKE_INSTALL_PREFIX=`pwd`/../llvm-install ../llvm-project/llvm/
# Build
$ ninja -j6 && ninja install && cd .. && rm -rf llvm-project llvm-buildk
```

#### Configuring and building SpecAST

When the repository is cloned, it is recommended it be cloned with the `--recursive` flag, to ensure that the proper submodules are automatically cloned. Alternatively, this can be done via a subsequent `git submodule init` command in the repo root folder.

First step is building the `library-metamorphic-testing` submodule:

```
$ cd third_party/library-metamorphic-testing/ && mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -DMETALIB_LIB_ONLY=ON -DYAML_BUILD_SHARED_LIBS=ON ..
$ make -j4
```

...and exposing the library in the SpecAST project:

```
$ cd ../../../ && mkdir libs
$ ln -s `pwd`/third_party/library-metamorphic-testing/build/libmetalib_fuzz.so ./libs
```

(**optional**) In case llvm/clang were built from source, then we need to set the `Clang_DIR` environment variable to the folder containing `ClangConfig.cmake`. This is generally `<llvm_install_folder>/lib/cmake/clang`.

The final step is to configure and build the SpecAST project:

```
$ mkdir build && cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=<path/to/clang> -DCMAKE_CXX_COMPILER=<path/to/clang++> ..
$ ninja -j4
```

## Testing a C++ library

#### Preparation

The first step is to have an installed version of the library to be tested. This
is assumed to exist, and that the library has some headers files which declare
library functions.

In order to make the tool aware of all compile flags, compile
commands should be exported. This is most easily done by preparing a dummy
`CMakeLists.txt`. If the library under test provides its own `cmake` or
`pkgconfig` module, then it is a matter of finding the module specification,
either via `find_package` for a `cmake` module, or via `pkg_search_module` for a
`pkgconfig` module. Alternatively, the correct flags can be passed via
`target_include_directories`, if no available module is provided.

Finally, it is sufficient to only compile with the `OBJECT` flag, as that will
produce the required `compile_commands.json` file.

**EXAMPLE FILE**: https://www.doc.ic.ac.uk/~al2510/SpecASTInputs/isl_point.tar/isl_point/CMakeLists.txt

#### Specification

Library functions and types that should be used during the test generation must
be made known via an `expose` annotation (i.e. add the
`__attribute__((annotate("expose")))` attribute where appropriate). An
additional expose directive, `expose_special` is provided, in the case where a
function should be made aware to the testing generator, as it is required when
specifying metamorphic relations, but should not be used for fuzzing.

Once whatever parts of the library should be used for testing are decided and
exposed appropriately, a fuzzing template should be provided, which outlines the way input variables to the metamorphic test are generated, such that they might present interesting features. There are three components to this file:

* A required `main` definition, where input variables to be used in the metamorphic testing phase (named *metamorphic input variable*) are declared and built, as well as a sketch to how the eventual test case should look. The fuzzer will use this to create fuzzed instructions, as specified by the template.
* A separate include file, which includes the library under test header files with the appropriate constructs exposed, and declares the fuzzing constructs used (**TODO** this could be generic and just include the library headers in the template file; **EXAMPLE FILE**: https://www.doc.ic.ac.uk/~al2510/SpecASTInputs/isl_point.tar/isl_point/spec_fuzz_point.hpp).
* An optional namespace `fuzz::lib_helper_funcs` with additional helper function definitions which might be useful to provide to the fuzzer to use, or be used when declaring metamorphic relations.


There are special functions that can be used to indicate that the fuzzer should
perform special actions, or use certain objects. Currently, these should be declared (but not defined) in a separate header file, included in the fuzzing template. These are:

* `fuzz::output_var` refers to the input variables to metamorphic relations. They are mainly used to ensure these objects are generated in such a way to have specific properties, depending on the library under test (e.g. are non-empty, would be a convex set, represent a bit-vector).
* `fuzz::fuzz_new<T>` should generate a new object of type `T`, where `T` has been exposed in the library specification.
* `fuzz::start()` and `fuzz::end()` represent boundaries to those instructions which are used to build one input metamorphic variable. As there might be some setup required, or some operations that we would like to perform across all variables, which should not be fuzzed, these are used to determine which instructions should be duplicated to provide multiple instances of metamorphic input variables with the same expected properties.
* `fuzz::meta_test()` represents a function call that will be replaced with a metamorphic testing sequence.

**EXAMPLE FILE**: https://www.doc.ic.ac.uk/~al2510/SpecASTInputs/isl_point.tar/isl_point/isl_point.cpp

**TODO**: Add metamorphic relation specification description

**OLD INTERFACE EXAMPLE FILE**: https://www.doc.ic.ac.uk/~al2510/SpecASTInputs/isl_point.tar/isl_point/set_meta_tests_isl.yaml

#### Testing

The final step is to put everything together and generate a test file, which can
then be compiled against a library under test installation and executed, to see
if any errors arise. The `mtFuzzer` binary needs to be provided with the fuzzer
and metamorphic specification files, as well as the exposed library header
files. The binary takes exactly one argument, being the fuzzer specification
file, and two required arguments: `--lib-list` represents a comma-separated list
of library header files, at least one of which containing exposed library
constructs, and `--set-meta-path` representing the path to a metamorphic
specification file. Additional optional arguments can be seen by passing
`--help`.

A test file should then be generated as `test.cpp` by default, which can then be
compiled and executed, where an non-zero return code represents a potential
misbehaviour of the library under test.
