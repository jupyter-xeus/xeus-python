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

## Advanced Configuration

```{warning}
This section is mostly for reference and should not be needed for regular use of the `jupyterlite-xeus-python` kernel.
```

### Provide a custom `empack_config.yaml`

Packages sometimes ship more data than needed for the package to work (tests, documentation, data files etc). This is fine on a regular installation of the package, but in the emscripten case when running in the browser this means that starting the kernel would download more files.
For this reason, `empack` filters out anything that is not required for the Python code to run. It does it by following a set of filtering rules available in this file: https://github.com/emscripten-forge/empack/blob/main/config/empack_config.yaml.

But this default filtering could break some packages. In that case you would probably want to either contribute to the default empack config, or provide your own set of filtering rules.

The xeus-python kernel supports passing a custom `empack_config.yaml`. This file can be used to override the default filter rules set by the underlying `empack` tool used for packing the environment.

If you would like to provide additional rules for including or excluding files in the packed environment, create a `empack_config.yaml` with the following content as an example:

```yaml
packages:
  xarray:
    include_patterns:
      - pattern: '**/*.py'
      - pattern: '**/static/css/*.css'
      - pattern: '**/static/html/*.html'
```

This example defines a set of custom rules for the `xarray` package to make sure it includes some static files that should be available from the kernel.

You can use this file when building JupyterLite:

```shell
jupyter lite build --XeusPythonEnv.empack_config=empack_config.yaml
```

```{note}
Filtering files helps reduce the size of the assets to download and as a consequence reduce network traffic.
```
