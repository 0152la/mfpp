# SpecAST

### Requirements

* LLVM >= 8.0
* Clang >= 8.0
* CMake >= 3.13

For `run_experiments.py`:
* Python >= 3.6
* pyyaml
* git-python

### Installation

First step is to retrieve and build the required submodules:

```
> git submodule update --init --recursive
> cd ./third_party/library_metamorphic_testing
> mkdir build && cd build
> cmake -DCMAKE_BUILD_TYPE=Release -DMETALIB_LIB_ONLY=True -DYAML_BUILD_SHARED=On ..
```

`DMETALIB_LIB_ONLY` ensures that we only build the required library file from
the submodule. The resulting library file needs to be added to the main project:

```
> cd ${SPEC_AST_ROOT}
> mkdir libs && cd libs
> ln -s ../third_party/library_metamorphic_testing/build/libmetalib_fuzz.so
```

Now, we can build the main project. If Clang is installed in a non-system location, **Clang_DIR** can be set to the directory containing **ClangConfig.cmake**. Otherwise, it should be automatically picked up.

```
> mkdir build && cd build
> cmake ..
```

At this point, with an appropriate specification, the tool can be used to generate test files. The usual command line is, from the root folder:

```
./build/mtFuzzer <TEMPLATE_LOCATION> -o <OUTPUT_LOCATION> --seed N
```

More information about each file can be found in the [SpecASTSpecs
repo](https://github.com/0152la/SpecASTSpecs).

### Running experiments

The provided `run_experiments.py` script provides a wrapper to generating,
compiling and executing a batch of tests. To help with this, a set of
specifications for a number of libraries are available
[here](https://github.com/0152la/SpecASTSpecs). In addition, a yaml file is
required as a parameter to `run_experiments.py`, for which a template can be
found in `./scripts/template.yaml`. Specifications in `SpecASTSpecs` come with
their own `yaml` files, for which paths must be updated. The following
placeholders should be updated appropriately (search/replace should suffice):

* **\<SPEC_AST_ROOT_DIR\>** - absolute location of the SpecAST repo
* **\<SPEC_NAME\>** - name for specification, i.e., a subfolder of **SpecASTSpecs**
* **\<LIB_NAME\>** - library name and name for template file
* **\<LIB_DIR\>** - absolute location to library repo, if appropriate

For a full list of parameters to `run_experiments.py`, execute with `--help`.
The following parameters are of interest:

* **mode** and **mode-val** - sets the execution mode; can be either **count**,
**time** or **generate**; **mode-val** is an additional argument which affects
the mode being used for execution
* **--stop-on-fail** - if set, testing stops whenever an execution fail is
triggered
* **--log-all-tests** - if set, all tests are logged, instead of only the
failing ones
* **--debug** - emit debug information to stdout
