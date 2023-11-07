# jupyterlite-xeus-python

[![ci-badge]][ci] [![docs-badge]][docs]

[ci-badge]: https://github.com/jupyterlite/xeus-python-kernel/workflows/Build/badge.svg
[ci]: https://github.com/jupyterlite/xeus-python-kernel/actions?query=branch%3Amain
[docs-badge]: https://readthedocs.org/projects/xeus-python-kernel/badge/?version=latest
[docs]: https://xeus-python-kernel.readthedocs.io/en/latest/?badge=latest

The [xeus-python](https://github.com/jupyter-xeus/xeus-python) Python kernel for JupyterLite running in the browser.

![jupyterlite-xeus-python](https://user-images.githubusercontent.com/21197331/167814755-76975633-30f7-4f8e-8fdb-eeec98fa3fd1.gif)

## Install

You can install the kernel with conda/mamba:

```bash
mamba install -c conda-forge jupyterlite-xeus-python
```

Or using pip:

```bash
pip install jupyterlite-xeus-python
```

Then build your JupyterLite site:

```bash
jupyter lite build
```

## Pre-installed packages

xeus-python allows you to pre-install packages in the Python runtime. You can pre-install packages by adding an `environment.yml` file in the JupyterLite build directory, this file will be found automatically by xeus-python which will pre-build the environment when running `jupyter lite build`.

Furthermore, this automatically installs any labextension that it founds, for example installing ipyleaflet will make ipyleaflet work without the need to manually install the jupyter-leaflet labextension.

Say you want to install `NumPy`, `Matplotlib` and `ipycanvas`, it can be done by creating the `environment.yml` file with the following content:

```yml
name: xeus-python-kernel
channels:
  - https://repo.mamba.pm/emscripten-forge
  - conda-forge
dependencies:
  - numpy
  - matplotlib
  - ipycanvas
```

Then you only need to build JupyterLite:

```bash
jupyter lite build
```

You can also pick another name for that environment file (_e.g._ `custom.yml`), by doing so, you will need to specify that name to xeus-python:

```bash
jupyter lite build --XeusPythonEnv.environment_file=custom.yml
```

## Contributing

### Development install

Note: You will need NodeJS to build the extension package.

The `jlpm` command is JupyterLab's pinned version of
[yarn](https://yarnpkg.com/) that is installed with JupyterLab. You may use
`yarn` or `npm` in lieu of `jlpm` below.

```bash
# Clone the repo to your local environment
# Change directory to the jupyterlite-xeus-python directory
# Install package in development mode
python -m pip install -e ".[dev]"

# Link your development version of the extension with JupyterLab
jupyter labextension develop . --overwrite

# Rebuild extension Typescript source after making changes
jlpm run build
```

You can watch the source directory and run JupyterLab at the same time in different terminals to watch for changes in the extension's source and automatically rebuild the extension.

```bash
# Watch the source directory in one terminal, automatically rebuilding when needed
jlpm run watch
# Run JupyterLab in another terminal
jupyter lab
```

With the watch command running, every saved change will immediately be built locally and available in your running JupyterLab. Refresh JupyterLab to load the change in your browser (you may need to wait several seconds for the extension to be rebuilt).

### Development uninstall

```bash
pip uninstall jupyterlite-xeus-python
```

In development mode, you will also need to remove the symlink created by `jupyter labextension develop`
command. To find its location, you can run `jupyter labextension list` to figure out where the `labextensions`
folder is located. Then you can remove the symlink named `jupyterlite-xeus-python` within that folder.

### Packaging the extension

See [RELEASE](RELEASE.md)
