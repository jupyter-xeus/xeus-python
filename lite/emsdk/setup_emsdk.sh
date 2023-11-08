#!/bin/bash

# first argument is the emscripten version to install
EMSCRIPTEN_VERSION=$1
# second argument is path to clone emsdk to
EMSDK_PATH=$2

# check if emscripten version is set
if [ -z "$EMSCRIPTEN_VERSION" ]; then
    echo "No emscripten version set. The first argument of this script should be the emscripten version to install."
    exit 1
fi

# check if emsdk path is set
if [ -z "$EMSDK_PATH" ]; then
    echo "No emsdk path set. The second argument of this script should be the path to clone emsdk to."
    exit 1
fi


# dir of this file
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PATCH_DIR=$SCRIPT_DIR/patches


set -e

echo "------------------------------------"
echo "Installing emsdk"
echo "------------------------------------" 


echo "...cloning emsdk"
git clone --depth 1 https://github.com/emscripten-core/emsdk.git $EMSDK_PATH


pushd $EMSDK_PATH
echo "emsdk install ..."
./emsdk install --build=Release $EMSCRIPTEN_VERSION
echo "...done"

echo "emsdk patching"
pushd upstream/emscripten
cat $PATCH_DIR/*.patch | patch -p1 --verbose 
popd    
echo "...done"

echo "emsdk building ..."   
./emsdk install --build=Release $EMSCRIPTEN_VERSION ccache-git-emscripten-64bit
echo "...done"

echo "emsdk activating ..."
./emsdk activate --embedded --build=Release $EMSCRIPTEN_VERSION
echo "...done"
popd


echo "create .emsdkdir file in home directory ..."
echo $EMSDK_PATH > ~/.emsdkdir
echo "...done"