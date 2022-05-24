xeus-python in JupyterLite
==========================

The `xeus-python <https://github.com/jupyter-xeus/xeus-python>`_ kernel compiled to wasm and installable in JupyterLite!!

Features:

- all IPython features included (magics, matplotlib inline `etc`)
- code completion
- code inspection
- interactive widgets (ipywidgets, ipyleaflet, bqplot, ipycanvas `etc`)
- ``from time import sleep`` works!
- pre-installed packages: you can install the packages you want in your xeus-python lite installation (see :ref:`configuration` page)

.. replite::
   :kernel: xeus-python
   :height: 600px

    print("Hello from xeus-python!")

.. toctree::
    :caption: Installation
    :maxdepth: 2

    installation
    configuration
