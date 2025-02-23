# Build and Run

## Dependencies

- Linux
- Python (>= 3.8, for the Python frontend)
- GCC (>= 10, to support C++20 and the "unroll" pragma)
- CUDA (>= 11.4.1, to support GCC 10, Optional)
- MKL (Optional)
- PyTorch (Optional, see below)
- Java (= 11, Build-time dependency only)

Other Python dependencies:

```sh
pip3 install --user numpy sourceinspect astor Pygments xgboost
```

!!! note "Note on Python version"
    Because we are analyzing Python AST, which is sensitive to Python version, there may be potential bugs for Python strictly later than 3.8. Please file an issue if something goes wrong

!!! note "PyTorch support"
    FreeTensor can optionally link PyTorch to support a copy-free interface between FreeTensor and PyTorch. Please note that, if you are using CUDA, FreeTensor and PyTorch should link CUDA
    of *the same version*. PyTorch can be installed in any way you like, see [PyTorch's guide](https://pytorch.org/get-started/locally/).

## Build

First, clone this repo. Don't forget there are some submodules.

```sh
git clone --recursive <path/to/this/repo>
```

Then, build.

```sh
mkdir build
cd build
cmake ..
make -j  # Or use Ninja
```

There are some options to `cmake`:

- `-DFT_WITH_CUDA=ON/OFF`: build with/without CUDA (defaults to `ON`).
- `-DFT_WITH_MKL=<path/to/mkl/root>`: build with MKL (path to MKL is required, defaults to building without it).

    The path accepts by CMake should be a raw unescaped path; i.e. `-DFT_WITH_MKL="/some path"` is good since the quotes are resolved by the shell but `-DFT_WITH_MKL=\"/some\ path\"` is not.

- `-DFT_WITH_PYTORCH=ON/OFF`: build with/without copy-free interface from/to PyTorch, requring PyTorch installed on the system (defaults to `OFF`).
- `-DFT_DEBUG_LOG_NODE=ON` (for developers): enables tracing to tell by which pass a specific AST node is modified.
- `-DFT_DEBUG_PROFILE=ON` (for developers): profiles some heavy functions in the compiler.
- `-DFT_DEBUG_SANITIZE=<sanitizer_name>` (for developers): build with GCC sanitizer (set it to a sanitizer name to use, e.g. address).

It will build a shared library with a name like `freetensor_ffi.cpython-37m-x86_64-linux-gnu.so`, which can be used in Python via `import freetensor`.

## Run a Program with FreeTensor

To run any program with FreeTensor, one should add the `python/` and `build/` directory to `PYTHONPATH` first.

E.g. to run a python program `a.py` with FreeTensor in the `build/` directory,

```sh
PYTHONPATH=../python:../build:$PYTHONPATH python3 a.py
```

## Global Configurations

There are serveral global configurations can be set via environment variables:

- `FT_PRETTY_PRINT=ON/OFF`. Enable/disable colored printing.
- `FT_PRINT_ALL_ID=ON/OFF`. Print (or not) IDs of all statements in an AST.
- `FT_WERROR=ON/OFF`. Treat warnings as errors (or not).
- `FT_BACKEND_COMPILER_CXX=<path/to/compiler>`. The C++ compiler used to compiler the optimized program. Default to the same compiler found when building FreeTensor itself, and compilers found in the `PATH` enviroment variable. This environment variable should be set to a colon-separated list of paths, in which the paths are searched from left to right.
- `FT_BACKEND_COMPILER_NVCC=<path/to/compiler>`. The CUDA compiler used to compiler the optimized program (if built with CUDA). Default to the same compiler found when building FreeTensor itself, and compilers found in the `PATH` enviroment variable. This environment variable should be set to a colon-separated list of paths, in which the paths are searched from left to right.

- `FT_DEBUG_BINARY=ON` (for developers). Compile with `-g` at backend. Do not delete the binary file after loaded.

This configurations can also set at runtime in [`ft.config`](../../api/#freetensor.core.config).

## Run the Tests

To run the test, first change into the `test/` directory, then

```sh
PYTHONPATH=../python:../build:$PYTHONPATH pytest
```

To run a single test case, specify the test case name, and optionally use `pytest -s` to display the standard output. E.g,

```sh
PYTHONPATH=../python:../build:$PYTHONPATH pytest -s 00.hello_world/test_basic.py::test_hello_world
```

!!! note "Debugging (for developers)"

    If using GDB, one should invoke PyTest with `python3 -m`:

    ```
    PYTHONPATH=../python:../build:$PYTHONPATH gdb --args python3 -m pytest
    ```

    If using Valgrind, one should set Python to use the system malloc:

    ```
    PYTHONPATH=../python:../build:$PYTHONPATH PYTHONMALLOC=malloc valgrind python3 -m pytest
    ```

    Sometimes Valgrind is not enough to detect some errors. An alternative is to use the sanitizer from GCC. For example, if you are using the "address" sanitizer, first set `-DFT_DEBUG_SANITIZE=address` to `cmake`, and then:

    ```
    PYTHONPATH=../python:../build:$PYTHONPATH LD_PRELOAD=`gcc -print-file-name=libasan.so` pytest -s
    ```

    If you are using another sanitizer, change the string set to `FT_DEBUG_SANITIZE` and the library's name. For example, `-DFT_DEBUG_SANITIZE=undefined` and `libubsan.so`.

## Build this Document

First install some dependencies:

```sh
pip3 install --user mkdocs mkdocstrings==0.18.1 "pytkdocs[numpy-style]"
```

From the root directory of FreeTensor, run a HTTP server to serve the document (recommended, but without document on C++ interface due to [a limitation](https://github.com/mkdocs/mkdocs/issues/1901)):

```sh
PYTHONPATH=./python:./build:$PYTHONPATH mkdocs serve
```

Or build and save the pages (with document on C++ interface, requiring Doxygen and Graphviz):

```sh
doxygen Doxyfile && PYTHONPATH=./python:./build:$PYTHONPATH mkdocs build
```

!!! note "Publish the documents to GitHub Pages (for developers)"

    `doxygen Doxyfile && PYTHONPATH=./python:./build:$PYTHONPATH mkdocs gh-deploy`
