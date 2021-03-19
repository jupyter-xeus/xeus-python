# ![xeus-python](docs/source/xeus-python.svg)

[![Azure Pipelines](https://dev.azure.com/jupyter-xeus/jupyter-xeus/_apis/build/status/jupyter-xeus.xeus-python?branchName=master)](https://dev.azure.com/jupyter-xeus/jupyter-xeus/_build/latest?definitionId=2&branchName=master)
[![Appveyor](https://ci.appveyor.com/api/projects/status/vy6rhqdw24pjduip?svg=true)](https://ci.appveyor.com/project/jupyter-xeus/xeus-python)
[![Documentation Status](http://readthedocs.org/projects/xeus-python/badge/?version=latest)](https://xeus-python.readthedocs.io/en/latest/?badge=latest)
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/jupyter-xeus/xeus-python/stable?urlpath=/lab/tree/notebooks/xeus-python.ipynb)
[![Join the Gitter Chat](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/QuantStack/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

`xeus-python` is a Jupyter kernel for Python based on the native implementation of the Jupyter protocol [xeus](https://github.com/jupyter-xeus/xeus).

## Usage

Launch the Jupyter notebook with `jupyter notebook` or Jupyter lab with `jupyter lab` and launch a new Python notebook by selecting the **xpython** kernel.

**Code execution and variable display**:

![Basic code execution](docs/source/code_exec.gif)

**Output streams**:

![Streams](docs/source/streams.gif)

**Input streams**:

![Input](docs/source/input.gif)

**Error handling**:

![Erro handling](docs/source/error.gif)

**Inspect**:

![Inspect](docs/source/inspect.gif)

**Code completion**:

![Completion](docs/source/code_completion.gif)

**Rich display**:

![Rich display](docs/source/rich_disp.gif)

**And of course widgets**:

![Widgets](docs/source/widgets.gif)
![Widgets binary](docs/source/binary.gif)


## Installation

xeus-python has been packaged for the conda package manager.

To ensure that the installation works, it is preferable to install `xeus-python` in a fresh conda environment. It is also needed to use a [miniconda](https://conda.io/miniconda.html) installation because with the full [anaconda](https://www.anaconda.com/) you may have a conflict with the `zeromq` library which is already installed in the anaconda distribution.


The safest usage is to create an environment named `xeus-python` with your miniconda installation

```bash
conda create -n xeus-python
conda activate xeus-python # Or `source activate xeus-python` for conda < 4.6
```

### Installing from conda

Then you can install in this environment `xeus-python` and its dependencies

```bash
conda install xeus-python notebook -c conda-forge
```

### Installing from PyPI

Depending on the platform, PyPI wheels may be available for xeus-python.

```bash
pip install xeus-python notebook
```

However, the wheels uploaded on PyPI are **experimental**.

In general we strongly recommend using a package manager instead. We maintain the conda-forge package,
and nothing prevents you from creating a package your favorite Linux distribution or FreeBSD.

The ongoing effort to package xeus-python for pip takes place in the [xeus-python-wheel](https://github.com/jupyter-xeus/xeus-python-wheel) repository.

### Installing from source

Or you can install it from the sources, you will first need to install dependencies

```bash
conda install cmake xeus nlohmann_json cppzmq xtl pybind11 pybind11_json ipython debugpy jupyterlab -c conda-forge
```

Then you can compile the sources (replace `$CONDA_PREFIX` with a custom installation prefix if need be)

```bash
mkdir build && cd build
cmake .. -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib -D PYTHON_EXECUTABLE=`which python`
make && make install
```

## Trying it online

To try out xeus-python interactively in your web browser, just click on the binder
link:

[![Binder](binder-logo.svg)](https://mybinder.org/v2/gh/jupyter-xeus/xeus-python/stable?urlpath=/lab/tree/notebooks/xeus-python.ipynb)

## Documentation

To get started with using `xeus-python`, check out the full documentation

http://xeus-python.readthedocs.io

## What are the advantages of using xeus-python over ipykernel (IPython kernel)?

Check-out this blog post for the answer: https://blog.jupyter.org/a-new-python-kernel-for-jupyter-fcdf211e30a8.
Long story short:

- xeus-python is a lot lighter than ipykernel, which makes it a lot easier to implement new features on top of it.
- xeus-python already works with the **Jupyter Lab debugger**: https://github.com/jupyterlab/debugger
- xeus-based kernels are more versatile in that one can overload e.g. the concurrency model. This is something that Kitware’s SlicerJupyter project takes advantage of to integrate with the Qt event loop of their Qt-based desktop application.

## Dependencies

``xeus-python`` depends on

 - [xeus](https://github.com/jupyter-xeus/xeus)
 - [xtl](https://github.com/xtensor-stack/xtl)
 - [pybind11](https://github.com/pybind/pybind11)
 - [pybind11_json](https://github.com/pybind/pybind11_json)
 - [nlohmann_json](https://github.com/nlohmann/json)


| `xeus-python`|   `xeus`         |      `xtl`      | `cppzmq` | `nlohmann_json` | `pybind11`     | `pybind11_json`   | `jedi`            | `pygments`        | `ptvsd` | `debugpy` | `IPython` |
|--------------|------------------|-----------------|----------|-----------------|----------------|-------------------|-------------------|-------------------|---------|-----------|-----------|
|  master      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.8,<0.3      |                   | >=2.3.1,<3.0.0    |         | >=1.1.0   | >=7.21,<8 |
|  0.12.3      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.8,<0.3      |                   | >=2.3.1,<3.0.0    |         | >=1.1.0   | >=7.20,<8 |
|  0.12.2      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.8,<0.3      |                   | >=2.3.1,<3.0.0    |         | >=1.1.0   | >=7.20,<8 |
|  0.11.1      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.8,<0.3      |                   | >=2.3.1,<3.0.0    |         | >=1.1.0   | >=7.20,<8 |
|  0.11.0      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.8,<0.3      |                   | >=2.3.1,<3.0.0    |         | >=1.1.0   | >=7.20,<8 |
|  0.10.2      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.19    | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.10.1      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.18.0,<0.19    | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.10.0      |  >=1.0.0,<0.26   |  >=0.7.0,<0.8   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.18.0,<0.19    | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.9.5       |  >=0.25.3,<0.26  |  >=0.6.23,<0.7  | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.18.0,<0.19    | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.9.4       |  >=0.25.3,<0.26  |  >=0.6.23,<0.7  | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.18    | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.9.3       |  >=0.25.3,<0.26  |  >=0.6.23,<0.7  | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.15.1          | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.9.2       |  >=0.25.3,<0.26  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.6.0,<3.0   | >=0.2.6,<0.3      | >=0.15.1          | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.9.1       |  >=0.25.0,<0.26  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1          | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.9.0       |  >=0.25.0,<0.26  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1          | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.8.7       |  >=0.24.2,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1          | >=2.3.1,<3.0.0    |         | >=1.1.0   |           |
|  0.8.6       |  >=0.24.2,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.16.0  | >=2.3.1,<3.0.0    | >=4.3.2 |           |           |
|  0.8.5       |  >=0.24.2,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.16.0  | >=2.3.1,<3.0.0    | >=4.3.2 |           |           |
|  0.8.4       |  >=0.24.1,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.16.0  | >=2.3.1,<3.0.0    | >=4.3.2 |           |           |
|  0.8.3       |  >=0.24.1,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.16.0  | >=2.3.1,<3.0.0    | >=4.3.2 |           |           |
|  0.8.2       |  >=0.24.1,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.16.0  | >=2.3.1,<3.0.0    | >=4.3.2 |           |           |
|  0.8.1       |  >=0.24.1,<0.25  |  >=0.6.8,<0.7   | ~4.4.1   | >=3.6.1,<4.0    | >=2.2.4,<3.0   | >=0.2.6,<0.3      | >=0.15.1,<0.16.0  | >=2.3.1,<3.0.0    | >=4.3.2 |           |           |


## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) to know how to contribute and set up a development environment.

## License

We use a shared copyright model that enables all contributors to maintain the
copyright on their contributions.

This software is licensed under the BSD-3-Clause license. See the [LICENSE](LICENSE) file for details.
