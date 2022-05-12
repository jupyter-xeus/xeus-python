# TODO Try to combine micromamba and emsdk
# FROM emscripten/emsdk:2.0.32
FROM mambaorg/micromamba:0.22.0

ARG MAMBA_DOCKERFILE_ACTIVATE=1

USER root
RUN apt-get update && apt-get install -y cmake
USER $MAMBA_USER

RUN micromamba install --yes -c https://repo.mamba.pm/conda-forge \
    git pip python=3.10 click typer

##################################################################
# Install emboa
##################################################################

RUN pip install git+https://github.com/emscripten-forge/emboa

##################################################################
# Setup emsdk
##################################################################

RUN git clone https://github.com/emscripten-core/emsdk.git && \
    pushd emsdk && \
    ./emsdk install 3.1.2 && \
    popd

##################################################################
# Create emscripten env
##################################################################

RUN micromamba create -n xeus-python-build-wasm \
    --platform=emscripten-32 \
    -c https://repo.mamba.pm/emscripten-forge \
    -c https://repo.mamba.pm/conda-forge \
    --yes \
    python=3.10 ipython pybind11 jedi xtl nlohmann_json \
    pybind11_json xeus "xeus-python-shell>=0.3" \
    numpy matplotlib

##################################################################
# git config
##################################################################
RUN git config --global advice.detachedHead false

##################################################################
# xeus-python build
##################################################################
# TODO Use a tag that is not master
RUN mkdir -p xeus-python && \
    git clone --branch master --depth 1 https://github.com/jupyter-xeus/xeus-python.git  xeus-python

RUN mkdir -p xeus-python/build && \
    cd xeus-python/build && \
    export PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-python-build-wasm && \
    export CMAKE_PREFIX_PATH=$PREFIX && \
    export CMAKE_SYSTEM_PREFIX_PATH=$PREFIX && \
    /tmp/emsdk/emsdk activate 3.1.2 && \
    source /tmp/emsdk/emsdk_env.sh && \
    emcmake cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \
        -DCMAKE_PROJECT_INCLUDE=cmake/overwriteProp.cmake \
        -DXPYT_EMSCRIPTEN_WASM_BUILD=ON \
        .. && \
    make -j8

RUN cd xeus-python && \
    python wasm_patches/patch_it.py

RUN cd xeus-python/build && \
    export FILE_PACKAGER=/tmp/emsdk/upstream/emscripten/tools/file_packager.py && \
    emboa pack python core $MAMBA_ROOT_PREFIX/envs/xeus-python-build-wasm --version=3.10

COPY copy_output.sh .

ENTRYPOINT ["/bin/bash"]
