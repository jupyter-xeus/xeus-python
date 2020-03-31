/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "pybind11/pybind11.h"

#include "xutils.hpp"

namespace py = pybind11;

namespace xpyt
{

    /****************************
     * matplotlib_inline module *
     ****************************/

    // This entire file should be removed as soon as the Matplotlib inline module is extracted from ipykernel

    py::module get_matplotlib_inline_module_impl()
    {
        py::module matplotlib_inline_module("matplotlib_inline");

        exec(py::str(R"(
# Copyright (c) IPython Development Team.
# Distributed under the terms of the Modified BSD License.

from __future__ import print_function
from io import BytesIO

import matplotlib
from matplotlib.backends.backend_agg import (
    FigureCanvasAgg,
)
from matplotlib import colors
from matplotlib._pylab_helpers import Gcf

from IPython.core.getipython import get_ipython
from IPython.core.display import display

from traitlets.config.configurable import SingletonConfigurable


def pil_available():
    out = False
    try:
        from PIL import Image
        out = True
    except:
        pass
    return out

# inherit from InlineBackendConfig for deprecation purposes
class InlineBackendConfig(SingletonConfigurable):
    pass

class InlineBackend(InlineBackendConfig):

    def __init__(self, **kwargs):
        self.rc = kwargs.get('rc', {
            'figure.figsize': (6.0, 4.0),
            'figure.facecolor': (1, 1, 1, 0),
            'figure.edgecolor': (1, 1, 1, 0),
            'font.size': 10,
            'figure.dpi': 72,
            'figure.subplot.bottom': .125
        })

        self.close_figures = True
        self.print_figure_kwargs = kwargs.get('print_figure_kwargs', {'bbox_inches': 'tight'})

        self._figure_formats = kwargs.get('figure_formats', {'png'})

    @property
    def figure_format(self):
        raise DeprecationWarning('use `figure_formats` instead')

    @figure_format.setter
    def figure_format(self, value):
        if value:
            self._figure_formats = {value}

    @property
    def figure_formats(self):
        return self._figure_formats

    @figure_formats.setter
    def figure_formats(self, value):
        if 'jpg' in value or 'jpeg' in value:
            if not pil_available():
                raise RuntimeError("Requires PIL/Pillow for JPG figures")


# Copied from IPython/core/pylabtools.py
def print_figure(fig, fmt, bbox_inches='tight', **kwargs):
    if not fig.axes and not fig.lines:
        return

    dpi = fig.dpi
    if fmt == 'retina':
        dpi = dpi * 2
        fmt = 'png'

    # build keyword args
    kw = {
        "format":fmt,
        "facecolor":fig.get_facecolor(),
        "edgecolor":fig.get_edgecolor(),
        "dpi":dpi,
        "bbox_inches":bbox_inches,
    }
    # **kwargs get higher priority
    kw.update(kwargs)

    bytes_io = BytesIO()
    if fig.canvas is None:
        from matplotlib.backend_bases import FigureCanvasBase
        FigureCanvasBase(fig)

    fig.canvas.print_figure(bytes_io, **kw)
    data = bytes_io.getvalue()
    if fmt == 'svg':
        data = data.decode('utf-8')
    return data


def show(close=None, block=None):
    if close is None:
        close = InlineBackend.instance().close_figures
    try:
        for figure_manager in Gcf.get_all_fig_managers():
            display(
                figure_manager.canvas.figure,
                metadata=_fetch_figure_metadata(figure_manager.canvas.figure)
            )
    finally:
        show._to_draw = []
        # only call close('all') if any to close
        # close triggers gc.collect, which can be slow
        if close and Gcf.get_all_fig_managers():
            matplotlib.pyplot.close('all')


# This flag will be reset by draw_if_interactive when called
show._draw_called = False
# list of figures to draw when flush_figures is called
show._to_draw = []


def draw_if_interactive():
    manager = Gcf.get_active()
    if manager is None:
        return
    fig = manager.canvas.figure

    if not hasattr(fig, 'show'):
        # Queue up `fig` for display
        fig.show = lambda *a: display(fig, metadata=_fetch_figure_metadata(fig))

    if not matplotlib.is_interactive():
        return

    try:
        show._to_draw.remove(fig)
    except ValueError:
        pass
    show._to_draw.append(fig)
    show._draw_called = True


def flush_figures():
    if not show._draw_called:
        return

    if InlineBackend.instance().close_figures:
        try:
            return show(True)
        except Exception as e:
            ip = get_ipython()
            if ip is None:
                raise e
            else:
                ip.showtraceback()
                return
    try:
        active = set([fm.canvas.figure for fm in Gcf.get_all_fig_managers()])
        for fig in [ fig for fig in show._to_draw if fig in active ]:
            try:
                display(fig, metadata=_fetch_figure_metadata(fig))
            except Exception as e:
                ip = get_ipython()
                if ip is None:
                    raise e
                else:
                    ip.showtraceback()
                    return
    finally:
        show._to_draw = []
        show._draw_called = False


FigureCanvas = FigureCanvasAgg

# def _enable_matplotlib_integration():
#     from matplotlib import get_backend
#     ip = get_ipython()
#     backend = get_backend()
#     if ip and backend == 'module://%s' % __name__:
#         from IPython.core.pylabtools import configure_inline_support, activate_matplotlib
#         try:
#             activate_matplotlib(backend)
#             configure_inline_support(ip, backend)
#         except (ImportError, AttributeError):
#             def configure_once(*args):
#                 activate_matplotlib(backend)
#                 configure_inline_support(ip, backend)
#                 ip.events.unregister('post_run_cell', configure_once)
#             ip.events.register('post_run_cell', configure_once)
#
# _enable_matplotlib_integration()

def _fetch_figure_metadata(fig):
    if _is_transparent(fig.get_facecolor()):
        ticksLight = _is_light([label.get_color()
                                for axes in fig.axes
                                for axis in (axes.xaxis, axes.yaxis)
                                for label in axis.get_ticklabels()])
        if ticksLight.size and (ticksLight == ticksLight[0]).all():
            return {'needs_background': 'dark' if ticksLight[0] else 'light'}

    return None

def _is_light(color):
    return rgbaArr[:,:3].dot((.299, .587, .114)) > .5

def _is_transparent(color):
    rgba = colors.to_rgba(color)
    return rgba[3] < .5
        )"), matplotlib_inline_module.attr("__dict__"));

        return matplotlib_inline_module;
    }

    py::module get_matplotlib_inline_module()
    {
        static py::module matplotlib_inline_module = get_matplotlib_inline_module_impl();
        return matplotlib_inline_module;
    }
}
