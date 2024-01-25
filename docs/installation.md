# Installation

## With mamba (or conda)

`xeus-python` has been packaged for the mamba (or conda) package manager.

To ensure that the installation works, it is preferable to install `xeus-python` in a fresh environment.
It is also needed to use a [miniforge](https://github.com/conda-forge/miniforge#mambaforge) or [miniconda](https://conda.io/miniconda.html) installation because with the full [anaconda](https://www.anaconda.com) you may have a conflict with
the `zeromq` library which is already installed in the anaconda distribution.


The safest usage is to create an environment named `xeus-python`

    mamba create -n xeus-python
    source activate xeus-python

Then you can install in this freshly created environment `xeus-python` and its dependencies

    mamba install xeus-python notebook -c conda-forge

or, if you prefer to use JupyterLab

    mamba install xeus-python jupyterlab -c conda-forge

## From PyPI

Depending on the platform, PyPI wheels may be available for xeus-python.

    pip install xeus-python notebook

However, the wheels uploaded on PyPI are **experimental**.

In general we strongly recommend using a package manager instead. We maintain the conda-forge package,
and nothing prevents you from creating a package your favorite Linux distribution or FreeBSD.

The ongoing effort to package xeus-python for pip takes place in the [xeus-python-wheel](https://github.com/jupyter-xeus/xeus-python-wheel) repository.

## From Source

You can install ``xeus-python`` from source with cmake. This requires that you have all the dependencies installed in the same prefix.

    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/path/to/prefix ..
    make install

On Windows platforms, from the source directory:

    mkdir build
    cd build
    cmake -G "NMake Makefiles" -DCMAKE_INSTALL_PREFIX=/path/to/prefix ..
    nmake
    nmake install

### Installing the Kernel Spec

When installing xeus-python in a given installation prefix, the corresponding Jupyter kernelspecs are installed in the same environment and are automatically picked up by Jupyter if it is installed in the same prefix.

However, if Jupyter is installed in a different location, it will not pick up the new kernel. The xeus-python can be registered with the following command:

    jupyter kernelspec install PREFIX/share/jupyter/kernels/xpython --sys-prefix

For more information on the ``jupyter kernelspec`` command, please consult the ``jupyter_client`` documentation.
