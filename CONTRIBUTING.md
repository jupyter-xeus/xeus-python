# Contributing to Xeus-python

Xeus and xeus-python are subprojects of Project Jupyter and subject to the [Jupyter governance](https://github.com/jupyter/governance) and [Code of conduct](https://github.com/jupyter/governance/blob/master/conduct/code_of_conduct.md).

## General Guidelines

For general documentation about contributing to Jupyter projects, see the [Project Jupyter Contributor Documentation](https://jupyter.readthedocs.io/en/latest/contributor/content-contributor.html).

## Community

The Xeus team organizes public video meetings. The schedule for future meetings and minutes of past meetings can be found on our [team compass](https://jupyter-xeus.github.io/).

## Setting up a development environment

First, you need to fork the project. Then setup your environment:

```bash
# create a new conda environment
conda create -f environment-dev.yml
conda activate xeus-python

# download xeus-python from your GitHub fork
git clone https://github.com/<your-github-username>/xeus-python.git
```

You may also want to install a C++ compiler, and cmake from conda if they are not available on your system.

## Building and installing xeus-python

```bash
# Create a directory for building
mkdir build && cd build
# Generate the makefile with cmake
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -DXPYT_DOWNLOAD_GTEST=ON -DPYTHON_EXECUTABLE=`which python` -DCMAKE_INSTALL_LIBDIR=lib ..
# Build and install
make install -j2
```

## Running the tests

To run the C++ test suite, from the build directory, type

```bash
./test/test_xeus-python
```

To run Python tests, from the build directory, type
```bash
cd ../test
pytest . -vvv
```

## Debugging the debugger

You can redirect all debugpy logs to a logs directory with:

```bash
export DEBUGPY_LOG_DIR=/home/path/to/logs
```

You can also enable xeus logging with:

```bash
export XEUS_LOG=1
```
