FROM mambaorg/micromamba:0.23.1

ARG MAMBA_DOCKERFILE_ACTIVATE=1
ARG PYTHON_VERSION=3.10

RUN micromamba install --yes -c conda-forge \
    git pip python=$PYTHON_VERSION click typer emsdk

##################################################################
# Install empack
##################################################################

RUN pip install empack>=0.5.2

##################################################################
# Setup emsdk
##################################################################

RUN emsdk install 3.1.2 && emsdk activate 3.1.2

##################################################################
# Create emscripten env and pack it
##################################################################

RUN micromamba create -n xeus-python-kernel \
    --platform=emscripten-32 \
    --root-prefix=/tmp/xeus-python-kernel \
    -c https://repo.mamba.pm/emscripten-forge \
    -c https://repo.mamba.pm/conda-forge \
    --yes \
    python=$PYTHON_VERSION xeus-python

RUN mkdir -p xeus-python-kernel && cd xeus-python-kernel && \
    cp /tmp/xeus-python-kernel/envs/xeus-python-kernel/bin/xpython_wasm.js . && \
    cp /tmp/xeus-python-kernel/envs/xeus-python-kernel/bin/xpython_wasm.wasm . && \
    empack pack python core /tmp/xeus-python-kernel/envs/xeus-python-kernel --version=$PYTHON_VERSION

COPY copy_output.sh .

ENTRYPOINT ["/bin/bash"]
