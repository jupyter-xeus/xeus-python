.. Copyright (c) 2017, Martin Renou, Johan Mabille, Sylvain Corlay, and
   Wolf Vollprecht

   Distributed under the terms of the BSD 3-Clause License.

   The full license is in the file LICENSE, distributed with this software.

Build and configuration
=======================

Build
-----

``xeus-python`` build supports the following options:

- ``BUILD_TESTS``: enables the ``xtest`` and ``xbenchmark`` targets (see below).
- ``DOWNLOAD_GTEST``: downloads ``gtest`` and builds it locally instead of using a binary installation.
- ``GTEST_SRC_DIR``: indicates where to find the ``gtest`` sources instead of downloading them.

The ``BUILD_TESTS`` and ``DOWNLOAD_GTEST`` options are disabled by default. Enabling ``DOWNLOAD_GTEST`` or
setting ``GTEST_SRC_DIR`` enables ``BUILD_TESTS``. If the ``BUILD_TESTS`` option is enabled, the `xtest` target is made available, which builds and run the test suite.

- ``XEUS_PYTHONHOME_RELPATH``: indicates the relative path of the PYTHONHOME with respect to the installation prefix.

By default, ``XEUS_PYTHONHOME_RELPATH`` is unset and the PYTHONHOME is set to the installation prefix, which is the expected behavior for most cases. A situation in which we may need to specify a different value for ``XEUS_PYTHONHOME_RELPATH`` is when using a Python installation from a different prefix. This occurs for example when building the conda package for xeus-python windows, since Python is installed in the general PREFIX while xeus-python is installed in the LIBRARY_PREFIX.

- ``XPYT_BUILD_SHARED``: Build the xeus-python shared library.
- ``XPYT_BUILD_STATIC``: Build the xeus-python static library.

By default, ``XPYT_BUILD_SHARED`` and ``XPYT_BUILD_STATIC`` are enabled by default.

- ``XPYT_USE_SHARED_XEUS``: Link xpython with the xeus shared library (instead of the static library).
- ``XPYT_USE_SHARED_XEUS_PYTHON``: Link xpython with the xeus-python shared library (instead of the static library).

By default, ``XPYT_USE_SHARED_XEUS`` and ``XPYT_USE_SHARED_XEUS_PYTHON`` are set to ``ON``.

- ``ENABLE_PYPI_WARNING``: we enable this option when building PyPI wheel to show a warning.
