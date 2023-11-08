#!/bin/bash
set -e

# EMSDK VERSION
EMSDK_VERSION="3.1.45"

# DIRECTORIES
LITE_SRC_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SRC_DIR=$LITE_SRC_DIR/..
EMSDK_SETUP_DIR=$LITE_SRC_DIR/emsdk
BUILD_ROOT_DIR=$LITE_SRC_DIR/build
mkdir -p $BUILD_ROOT_DIR
XEUS_PYTHON_BUILD_DIR=$BUILD_ROOT_DIR/xeus-python
EMSDK_INSTALL_DIR=$BUILD_ROOT_DIR/emsdk
WASM_ENV_PREFIX=$BUILD_ROOT_DIR/wasm-env
WASM_ENV_FILE=$SRC_DIR/environment-wasm-host.yml
SERVE_DIR=$BUILD_ROOT_DIR/serve

# find out number of cores
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    NPROC=$(nproc --all)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    NPROC=$(sysctl -n hw.ncpu)
fi

#############################
# Build/ setup emsdk
############################
mkdir -p $EMSDK_INSTALL_DIR
if [ ! -f "$EMSDK_INSTALL_DIR/emsdk_env.sh" ]; then
    echo "emsdk $EMSDK_VERSION not installed, installing it now"
    $EMSDK_SETUP_DIR/setup_emsdk.sh  $EMSDK_VERSION $EMSDK_INSTALL_DIR

else
    echo "skipping emsdk installation -- emsdk $EMSDK_VERSION already installed"
fi

#############################
# Create wasm env
############################
if [ ! -f "$WASM_ENV_PREFIX/lib/libxeus.a" ]; then
    rm -rf $WASM_ENV_PREFIX

    micromamba create -f $WASM_ENV_FILE  \
        --prefix $WASM_ENV_PREFIX \
        --platform=emscripten-wasm32 \
        -c https://repo.mamba.pm/emscripten-forge \
        -c https://repo.mamba.pm/conda-forge \
        --yes 
else
    echo "skipping env creation -- wasm env \"$WASM_ENV_PREFIX\" already exists"
fi

# ENSURE BUILD DIR EXISTS
mkdir -p $XEUS_PYTHON_BUILD_DIR

# remove fake python binary
rm -f $WASM_ENV_PREFIX/bin/python*

#############################
# Build xeus-python c++
############################

# activate emsdk
source $EMSDK_INSTALL_DIR/emsdk_env.sh

echo "building xpython_wasm"
echo "emcc" $(which emcc)

# let cmake know where the env is
export PREFIX=$WASM_ENV_PREFIX
export CMAKE_PREFIX_PATH=$PREFIX
export CMAKE_SYSTEM_PREFIX_PATH=$PREFIX

export LDFLAGS=""
export CFLAGS=""
export CXXFLAGS=""

emcmake $CONDA_PREFIX/bin/cmake \
    -S $SRC_DIR \
    -B $XEUS_PYTHON_BUILD_DIR \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
    -DCMAKE_INSTALL_PREFIX=$WASM_ENV_PREFIX \
    -DCMAKE_BUILD_TYPE=Release \
    -DXPYT_EMSCRIPTEN_WASM_BUILD=ON

pushd $XEUS_PYTHON_BUILD_DIR
emmake make -j$NPROC install
popd

# deactivate emsdk
source $EMSDK_INSTALL_DIR/emsdk_env.sh --deactivate



# #############################
# # Build Python
# ############################
# pushd $LITE_SRC_DIR
# export XEUS_PYTHON_LOCAL_BUILD=1
# export XEUS_PYTHON_BUILD_DIR=$XEUS_PYTHON_BUILD_DIR
# echo "building python package"
# python -m pip install -e ".[dev]"
# popd



# rm -rf $SERVE_DIR
# mkdir -p $SERVE_DIR
# pushd $SERVE_DIR
# jupyter lite build . 
# popd