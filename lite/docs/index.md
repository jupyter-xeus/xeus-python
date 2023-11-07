# xeus-python in JupyterLite 🚀🪐

The [xeus-python](https://github.com/jupyter-xeus/xeus-python) kernel compiled to WebAssembly and installable in JupyterLite!

Features:

- all IPython features included (magics, matplotlib inline `etc`)
- code completion
- code inspection
- interactive widgets (ipywidgets, ipyleaflet, bqplot, ipycanvas `etc`)
- JupyterLite custom file-system mounting

How does it compare to the [Pyodide kernel](https://github.com/jupyterlite/pyodide-kernel) for JupyterLite?

- `from time import sleep` works!
- starts faster!
- it's lighter by default!
- pre-installed packages! No more piplite (see {ref}`configuration` page)
- no more piplite, but we will be working on a mambalite, stay tuned :D

```{eval-rst}
.. replite::
   :kernel: xeus-python
   :height: 600px

   print("Hello from xeus-python!")
```

```{toctree}
:caption: Usage
:maxdepth: 2

installation
deploy
configuration
changelog
```
