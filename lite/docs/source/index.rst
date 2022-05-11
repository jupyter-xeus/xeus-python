xeus-python in JupyterLite
==========================

xeus-python is now compiled to wasm and installable in JupyterLite!!

Install
-------

To install the extension, execute:

.. code::

    pip install jupyterlite jupyterlite-xeus-python

    # This should pick up the xeus-python kernel automatically
    jupyter lite build

Try it online
-------------

You can try xeus-python directly in this documentation page!

.. replite::
   :kernel: xeus-python
   :height: 600px

    print("Hello from xeus-python!")
