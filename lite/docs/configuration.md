(configuration)=

# Configuration

## Pre-installed packages

`xeus-python` allows you to pre-install packages in the Python runtime. You can pre-install packages by adding an `environment.yml` file in the JupyterLite build directory, this file will be found automatically by xeus-python which will pre-build the environment when running `jupyter lite build`.

Furthermore, this automatically installs any labextension that it founds, for example installing ipyleaflet will make ipyleaflet work without the need to manually install the jupyter-leaflet labextension.

Say you want to install `NumPy`, `Matplotlib` and `ipycanvas`, it can be done by creating the `environment.yml` file with the following content:

```
name: xeus-python-kernel
channels:
- https://repo.mamba.pm/emscripten-forge
- https://repo.mamba.pm/conda-forge
dependencies:
- numpy
- matplotlib
- ipycanvas
```

Then you only need to build JupyterLite:

```
jupyter lite build
```

You can also pick another name for that environment file (*e.g.* `custom.yml`), by doing so, you will need to specify that name to xeus-python:

```
jupyter lite build --XeusPythonEnv.environment_file=custom.yml
```

```{note}
It is common to provide `pip` dependencies in a conda environment file. This is currently **not supported** by xeus-python, but there is a [work-in-progress](https://github.com/jupyterlite/xeus-python-kernel/pull/102) to support it.
```

Then those packages are usable directly:

```{eval-rst}
.. replite::
   :kernel: xeus-python
   :height: 600px
   :prompt: Try it!

   %matplotlib inline

   import matplotlib.pyplot as plt
   import numpy as np

   fig = plt.figure()
   plt.plot(np.sin(np.linspace(0, 20, 100)))
   plt.show();
```
