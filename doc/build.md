# Build piac from source

The main project [README.md](../README.md) outlines the simplest way to build
from source and what prerequisites are required. This page outlines other ways
building. The build is always two-stage: external dependencies, followed by
piac itself. The rationale is that the former can take long but is usually done
once, while the latter is frequent and, since it is independent of the former,
quick.

## Build in debug info instead of release/optimized

Specify the build type to cmake:

```sh
# Build external libraries
mkdir -p external/build && cd external/build && cmake .. && make && cd -
# Build piac
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make && cd -
# Test piac (optional)
cd build && ctest
```

## Build with the clang compilers

Specify the clang compilers to _both_ cmake calls:

```sh
# Build external libraries
mkdir -p external/build && cd external/build && cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .. && make && cd -
# Build piac
mkdir build && cd build && cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .. && make && cd -
# Test piac (optional)
cd build && ctest
```

## Build with ninja instead of make

You can also pass `-GNinja` to _both_ cmake calls:

```sh
# build external libraries
mkdir -p external/build && cd external/build && cmake -GNinja .. && make && cd -
# build piac
mkdir build && cd build && cmake -GNinja .. && make && cd -
# test piac (optional)
cd build && ctest
```

