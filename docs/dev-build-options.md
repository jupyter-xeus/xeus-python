# Build and configuration

## General Build Options

### Building the xeus-python library

``xeus-python`` build supports the following options:

- ``XPYT_BUILD_SHARED``: Build the xeus-python shared library. **Enabled by default**.
- ``XPYT_BUILD_STATIC``: Build the xeus-python static library. **Enabled by default**.

Xeus-python must link with xeus dynamically or statically.

- ``XPYT_USE_SHARED_XEUS``: Link with the xeus shared library (instead of the static library). **Enabled by default**.

### Building the kernel

The package includes two options for producing a kernel: an executable ``xpython`` and a Python extension module, which is used to launch a kernel from Python.

- ``XPYT_BUILD_XPYTHON_EXECUTABLE``: Build the xpython executable. **Enabled by default**.
- ``XPYT_BUILD_XPYTHON_EXTENSION``: Build the Python extension. **Disabled by default**.

The build of the Python extension is used in the context of the PyPI package to support virtual environments and user installations.

Both target make use of the two following options:

- ``XPYT_USE_SHARED_XEUS_PYTHON``: Link xpython with the xeus-python shared library. **Enabled by default**.

If ``XPYT_USE_SHARED_XEUS_PYTHON`` is disabled, xpython will be linked statically with xeus-python.

### Building the Tests

- ``XPYT_BUILD_TESTS``: enables the ``xtest`` and ``xbenchmark`` targets (see below). **Disabled by default**.
- ``XPYT_DOWNLOAD_GTEST``: downloads ``gtest`` and builds it locally instead of using a binary installation. **Disabled by default**.
- ``XPYT_GTEST_SRC_DIR``: indicates where to find the ``gtest`` sources instead of downloading them. **Unset by default**.

Enabling ``XPYT_DOWNLOAD_GTEST`` or setting ``XPYT_GTEST_SRC_DIR`` enables ``XPYT_BUILD_TESTS``. If the ``XPYT_BUILD_TESTS`` option is enabled, the `xtest` target is made available, which builds and runs the test suite.

### Other options

- ``XPYT_ENABLE_PYPI_WARNING``: We enable this option when building PyPI wheel to show a warning discouraging the use of PyPI. **Disabled by default**.
- ``XEUS_PYTHONHOME_RELPATH``: indicates the relative path of the PYTHONHOME with respect to the installation prefix of the ``xpython`` target. This variable is unset by default.
- ``XEUS_PYTHONHOME_ABSPATH``: indicates the absolute path of the PYTHONHOME for the ``xpython`` target. This variable is unset by default.

By default, ``XEUS_PYTHONHOME_RELPATH`` and ``XEUS_PYTHONHOME_ABSPATH`` are unset and the PYTHONHOME is set to the installation prefix, which is the expected behavior for most cases. A situation in which we may need to specify a different value for ``XEUS_PYTHONHOME_RELPATH`` or ``XEUS_PYTHONHOME_ABSPATH`` is when using a Python installation from a different prefix. This occurs for example when building the conda package for xeus-python windows, since Python is installed in the general ``PREFIX`` while xeus-python is installed in the ``LIBRARY_PREFIX``.

