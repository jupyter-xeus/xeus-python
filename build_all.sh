#bin/bash
set -e

# boa build /home/martin/github/recipes/recipes/xeus-python-shell --target-platform=emscripten-32

# rm -rf /home/martin/micromamba/envs/xeus-python-kernel
# micromamba create -n xeus-python-kernel --platform=emscripten-32 -c /home/martin/micromamba/envs/xeus-python/conda-bld/ --yes python ipython pybind11 jedi xtl nlohmann_json pybind11_json numpy xeus xeus-python-shell=0.3.0

rm -rf bld_ems
mkdir bld_ems
cd bld_ems

export PREFIX=/home/martin/micromamba/envs/xeus-python-kernel
export CMAKE_PREFIX_PATH=$PREFIX
export CMAKE_SYSTEM_PREFIX_PATH=$PREFIX

# export EMCC_FORCE_STDLIBS=1
emcmake cmake  \
    -DCMAKE_BUILD_TYPE=Release\
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
    -DCMAKE_PROJECT_INCLUDE=overwriteProp.cmake \
    -DXPYT_WASM_BUILD=ON \
    ..

make -j12

emboa pack python core /home/martin/micromamba/envs/xeus-python-kernel --version=3.10

cd ..
# patch
python patch_it.py

cp bld_ems/python_data.js       $HOME/github/xeus-python-kernel/src
cp bld_ems/python_data.data     $HOME/github/xeus-python-kernel/src
cp bld_ems/xpython_wasm.js       $HOME/github/xeus-python-kernel/src
cp bld_ems/xpython_wasm.wasm     $HOME/github/xeus-python-kernel/src

# cd $HOME/github/xeus-python-kernel
# yarn run build && jupyter lite build && jupyter lite serve
