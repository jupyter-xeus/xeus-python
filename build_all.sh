#bin/bash
set -e

boa build  $EM_RECIPES/xeus  --target-platform=emscripten-32

$MAMBA_EXE create  -n emxeus   --platform=emscripten-32  --yes python ipython pybind11 jedi xeus xtl nlohmann_json pybind11_json numpy

mkdir -p bld_ems
cd bld_ems




export PREFIX=$HOME/micromamba/envs/emxeus
export CMAKE_PREFIX_PATH=$PREFIX 
export CMAKE_SYSTEM_PREFIX_PATH=$PREFIX 
# export EMCC_FORCE_STDLIBS=1
emcmake cmake  \
    -DCMAKE_BUILD_TYPE=Release\
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
    -DCMAKE_PROJECT_INCLUDE=overwriteProp.cmake \
    -DXPYT_EMSCRIPTEN_WASM_BUILD=ON \
    ..

make -j12

emboa pack python core $HOME/micromamba/envs/emxeus --version=3.10


cd ..
# patch
python patch_it.py





cp bld_ems/python_data.js $HOME/src/xeus-python-kernel/src
cp bld_ems/python_data.data $HOME/src/xeus-python-kernel/src
cp bld_ems/xeus_kernel.js $HOME/src/xeus-python-kernel/src
cp bld_ems/xeus_kernel.wasm $HOME/src/xeus-python-kernel/src

cd $HOME/src/xeus-python-kernel
npm run build && jupyter lite build && jupyter lite serve
