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

You can also pass `-GNinja` to either or both cmake calls:

```sh
# build external libraries
mkdir -p external/build && cd external/build && cmake -GNinja .. && ninja && cd -
# build piac
mkdir build && cd build && cmake -GNinja .. && ninja && cd -
# test piac (optional)
cd build && ctest
```

## Build documentation

```sh
mkdir build && cd build && cmake -GNinja ..
ninja doc
firefox --new-tab doc/html/index.html
```

## Build test code coverage and static analysis reports

In debug mode:
```sh
mkdir build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=on ..
ninja test_coverage
firefox --new-tab doc/html/DEBUG/test_coverage/index.html
ninja cppcheck-xml
firefox --new-tab doc/html/DEBUG/cppcheck/index.html
```

In release/optimized mode:
```sh
mkdir build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCOVERAGE=on ..
ninja test_coverage
firefox --new-tab doc/html/RELEASE/test_coverage/index.html
ninja cppcheck-xml
firefox --new-tab doc/html/RELEASE/cppcheck/index.html
```
