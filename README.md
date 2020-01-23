# Building

```
git clone --recursive https://github.com/0152la/SpecAST
cd third_party/library-metamorphic-testing/ && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMETALIB_LIB_ONLY=ON -DYAML_BUILD_SHARED_LIBS=ON ..
make -j4 && cd ../../../ && mkdir libs
ln -s `pwd`/third_party/library-metamorphic-testing/build/libmetalib_fuzz.so ./libs
mkdir build && cd build
cmake -G "Ninja" ..
ninja
```