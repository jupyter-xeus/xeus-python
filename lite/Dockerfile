# TODO Try to combine micromamba and emsdk
# FROM emscripten/emsdk:2.0.32
FROM mambaorg/micromamba:0.22.0

ARG MAMBA_DOCKERFILE_ACTIVATE=1
ARG PYTHON_VERSION=3.10

RUN micromamba install --yes -c https://repo.mamba.pm/conda-forge \
    git pip python=$PYTHON_VERSION click typer

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
# Create emscripten env and pack it
##################################################################

RUN micromamba create -n xeus-python-kernel \
    --platform=emscripten-32 \
    -c https://repo.mamba.pm/emscripten-forge \
    -c https://repo.mamba.pm/conda-forge \
    --yes \
    python=$PYTHON_VERSION xeus-python \
    numpy matplotlib

RUN mkdir -p xeus-python-kernel && cd xeus-python-kernel && \
    export FILE_PACKAGER=/tmp/emsdk/upstream/emscripten/tools/file_packager.py && \
    /tmp/emsdk/emsdk activate 3.1.2 3.1.2 && \
    cp $MAMBA_ROOT_PREFIX/envs/xeus-python-kernel/bin/xpython_wasm.js . && \
    cp $MAMBA_ROOT_PREFIX/envs/xeus-python-kernel/bin/xpython_wasm.wasm . && \
    emboa pack python core $MAMBA_ROOT_PREFIX/envs/xeus-python-kernel --version=$PYTHON_VERSION

COPY copy_output.sh .

ENTRYPOINT ["/bin/bash"]
