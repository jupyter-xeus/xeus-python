/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus/xguid.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/stl.h"

#include "xeus-python/xutils.hpp"

#include "xdisplay.hpp"
#include "xinternal_utils.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    bool should_include(const std::string& mimetype, const std::vector<std::string>& include)
    {
        return include.size() == 0 || std::find(include.cbegin(), include.cend(), mimetype) != include.end();
    }

    bool should_exclude(const std::string& mimetype, const std::vector<std::string>& exclude)
    {
        return exclude.size() != 0 && std::find(exclude.cbegin(), exclude.cend(), mimetype) != exclude.end();
    }

    void compute_repr(
        const py::object& obj, const std::string& repr_method, const std::string& mimetype,
        const std::vector<std::string>& include, const std::vector<std::string>& exclude,
        py::dict& pub_data, py::dict& pub_metadata)
    {
        if (hasattr(obj, repr_method.c_str()) && should_include(mimetype, include) && !should_exclude(mimetype, exclude))
        {
            const py::object& repr = obj.attr(repr_method.c_str())();

            if (!repr.is_none())
            {
                if (py::isinstance<py::tuple>(repr))
                {
                    py::tuple repr_tuple = repr;

                    pub_data[mimetype.c_str()] = repr_tuple[0];
                    pub_metadata[mimetype.c_str()] = repr_tuple[1];
                }
                else
                {
                    pub_data[mimetype.c_str()] = repr;
                }
            }
        }
    }

    py::tuple mime_bundle_repr(const py::object& obj, const std::vector<std::string>& include = {}, const std::vector<std::string>& exclude = {})
    {
        py::module py_json = py::module::import("json");
        py::module builtins = py::module::import("builtins");
        py::dict pub_data;
        py::dict pub_metadata;

        if (hasattr(obj, "_repr_mimebundle_"))
        {
            auto bundle = obj.attr("_repr_mimebundle_")(include, exclude);

            if (py::isinstance<py::tuple>(bundle) || py::isinstance<py::list>(bundle))
            {
                py::list data_metadata = bundle;
                pub_data = data_metadata[0];
                pub_metadata = data_metadata[1];
            }
            else
            {
                pub_data = bundle;
            }
        }
        else
        {
            compute_repr(obj, "_repr_html_", "text/html", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_markdown_", "text/markdown", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_svg_", "image/svg+xml", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_png_", "image/png", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_jpeg_", "image/jpeg", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_latex_", "text/latex", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_json_", "application/json", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_javascript_", "application/javascript", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_pdf_", "application/pdf", include, exclude, pub_data, pub_metadata);
        }

        pub_data["text/plain"] = py::str(builtins.attr("repr")(obj));

        return py::make_tuple(pub_data, pub_metadata);
    }

    /****************************
     * xdisplayhook declaration *
     ****************************/

    class xdisplayhook
    {
    public:

        xdisplayhook();
        virtual ~xdisplayhook();

        void set_execution_count(int execution_count);
        void operator()(const py::object& obj, bool raw) const;

    private:

        int m_execution_count;
    };

    /*******************************
     * xdisplayhook implementation *
     *******************************/

    xdisplayhook::xdisplayhook()
        : m_execution_count(0)
    {
    }

    xdisplayhook::~xdisplayhook()
    {
    }

    void xdisplayhook::set_execution_count(int execution_count)
    {
        m_execution_count = execution_count;
    }

    void xdisplayhook::operator()(const py::object& obj, bool raw = false) const
    {
        auto& interp = xeus::get_interpreter();

        if (!obj.is_none())
        {
            if (hasattr(obj, "_ipython_display_"))
            {
                obj.attr("_ipython_display_")();
                return;
            }

            py::object pub_data;
            py::object pub_metadata;
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                const py::tuple& repr = mime_bundle_repr(obj);
                pub_data = repr[0];
                pub_metadata = repr[1];
            }

            interp.publish_execution_result(m_execution_count, pub_data, pub_metadata);
        }
    }

    /***************************
     * xdisplay implementation *
     ***************************/

    void xdisplay_impl(const py::args objs,
                       const std::vector<std::string>& include,
                       const std::vector<std::string>& exclude,
                       const py::dict& metadata,
                       const py::object& transient,
                       const py::object& display_id,
                       bool update,
                       bool raw)
    {
        auto& interp = xeus::get_interpreter();

        for (std::size_t i = 0; i < objs.size(); ++i)
        {
            py::object obj = objs[i];
            if (!obj.is_none())
            {
                if (hasattr(obj, "_ipython_display_"))
                {
                    obj.attr("_ipython_display_")();
                    return;
                }

                py::object pub_data;
                py::object pub_metadata = py::dict();
                if (raw)
                {
                    pub_data = obj;
                }
                else
                {
                    const py::tuple& repr = mime_bundle_repr(obj, include, exclude);
                    pub_data = repr[0];
                    pub_metadata = repr[1];
                }
                pub_metadata.attr("update")(metadata);

                nl::json cpp_transient = transient.is_none() ? nl::json::object() : nl::json(transient);

                if (!display_id.is_none())
                {
                    cpp_transient["display_id"] = display_id;
                }

                if (update)
                {
                    interp.update_display_data(pub_data, pub_metadata, std::move(cpp_transient));
                }
                else
                {
                    interp.display_data(pub_data, pub_metadata, std::move(cpp_transient));
                }
            }
        }
    }

    void xdisplay(py::args objs, py::kwargs kw)
    {
        auto raw = kw.contains("raw") ? kw["raw"].cast<bool>() : false;
        auto include = kw.contains("include") ? kw["include"].cast<std::vector<std::string>>() : std::vector<std::string>();
        auto exclude = kw.contains("exclude") ? kw["exclude"].cast<std::vector<std::string>>() : std::vector<std::string>({});
        auto metadata = kw.contains("metadata") ? py::object(kw["metadata"]) : py::dict();
        auto transient = kw.contains("transient") ? py::object(kw["transient"]) : py::none();
        auto display_id = kw.contains("display_id") ? py::object(kw["display_id"]) : py::none();
        auto update = kw.contains("update") ? py::bool_(kw["update"]).cast<bool>() : false;

        xdisplay_impl(objs, include, exclude, metadata, transient, display_id, update, raw);
    }

    void xupdate_display(py::args objs, py::kwargs kw)
    {
        auto raw = kw.contains("raw") ? kw["raw"].cast<bool>() : false;
        auto include = kw.contains("include") ? kw["include"].cast<std::vector<std::string>>() : std::vector<std::string>();
        auto exclude = kw.contains("exclude") ? kw["exclude"].cast<std::vector<std::string>>() : std::vector<std::string>({});
        auto metadata = kw.contains("metadata") ? py::object(kw["metadata"]) : py::dict();
        auto transient = kw.contains("transient") ? py::object(kw["transient"]) : py::none();
        auto display_id = kw.contains("display_id") ? py::object(kw["display_id"]) : py::none();

        xdisplay_impl(objs, include, exclude, metadata, transient, display_id, true, raw);
    }

    void xpublish_display_data(const py::object& data, const py::object& metadata, const py::str& /*source*/, const py::object& transient)
    {
        auto& interp = xeus::get_interpreter();

        interp.display_data(data, metadata, transient);
    }

    /*************************
     * xclear implementation *
     *************************/

    void xclear(bool wait = false)
    {
        auto& interp = xeus::get_interpreter();
        interp.clear_output(wait);
    }

    /******************
     * display module *
     ******************/

    py::module get_display_module_impl()
    {
        py::module display_module = create_module("display");

        py::class_<xdisplayhook>(display_module, "DisplayHook")
            .def(py::init<>())
            .def("set_execution_count", &xdisplayhook::set_execution_count)
            .def("__call__", &xdisplayhook::operator(), py::arg("obj"), py::arg("raw") = false);

        display_module.def("display", xdisplay);

        display_module.def("update_display", xupdate_display);

        display_module.def("publish_display_data",
            xpublish_display_data,
            py::arg("data"),
            py::arg("metadata") = py::dict(),
            py::arg("source") = py::str(),
            py::arg("transient") = py::dict());

        display_module.def("clear_output",
              xclear,
              py::arg("wait") = false);

        // This is a temporary copy of IPython.display module
        // When IPython 8 is out, we can monkey patch only the module containing the core display functions
        // See https://github.com/jupyter-xeus/xeus-python/pull/266
        exec(py::str(R"(
# -*- coding: utf-8 -*-
"""Top-level display functions for displaying object in different formats."""

# Copyright (c) IPython Development Team.
# Distributed under the terms of the Modified BSD License.


from binascii import b2a_hex, b2a_base64, hexlify
import json
import mimetypes
import os
import struct
import sys
import warnings
from copy import deepcopy
from os.path import splitext, exists, isfile, abspath, join, isdir
from os import walk, sep, fsdecode
from pathlib import Path, PurePath
from html import escape as html_escape


#-----------------------------------------------------------------------------
# utility functions
#-----------------------------------------------------------------------------

def decode(s, encoding=None):
    encoding = encoding or sys.getdefaultencoding()
    return s.decode(encoding, "replace")

def cast_unicode(s, encoding=None):
    if isinstance(s, bytes):
        return decode(s, encoding)
    return s

def _safe_exists(path):
    """Check path, but don't let exceptions raise"""
    try:
        return os.path.exists(path)
    except Exception:
        return False

def _merge(d1, d2):
    """Like update, but merges sub-dicts instead of clobbering at the top level.

    Updates d1 in-place
    """

    if not isinstance(d2, dict) or not isinstance(d1, dict):
        return d2
    for key, value in d2.items():
        d1[key] = _merge(d1.get(key), value)
    return d1

def _display_mimetype(mimetype, objs, raw=False, metadata=None):
    """internal implementation of all display_foo methods

    Parameters
    ----------
    mimetype : str
        The mimetype to be published (e.g. 'image/png')
    objs : tuple of objects
        The Python objects to display, or if raw=True raw text data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    if metadata:
        metadata = {mimetype: metadata}
    if raw:
        # turn list of pngdata into list of { 'image/png': pngdata }
        objs = [ {mimetype: obj} for obj in objs ]
    display(*objs, raw=raw, metadata=metadata, include=[mimetype])

#-----------------------------------------------------------------------------
# Main functions
#-----------------------------------------------------------------------------


def _new_id():
    """Generate a new random text id with urandom"""
    return b2a_hex(os.urandom(16)).decode('ascii')


class DisplayHandle(object):
    """A handle on an updatable display

    Call `.update(obj)` to display a new object.

    Call `.display(obj`) to add a new instance of this display,
    and update existing instances.

    See Also
    --------

        :func:`display`, :func:`update_display`

    """

    def __init__(self, display_id=None):
        if display_id is None:
            display_id = _new_id()
        self.display_id = display_id

    def __repr__(self):
        return "<%s display_id=%s>" % (self.__class__.__name__, self.display_id)

    def display(self, obj, **kwargs):
        """Make a new display with my id, updating existing instances.

        Parameters
        ----------

        obj:
            object to display
        **kwargs:
            additional keyword arguments passed to display
        """
        display(obj, display_id=self.display_id, **kwargs)

    def update(self, obj, **kwargs):
        """Update existing displays with my id

        Parameters
        ----------

        obj:
            object to display
        **kwargs:
            additional keyword arguments passed to update_display
        """
        update_display(obj, display_id=self.display_id, **kwargs)


def display_pretty(*objs, **kwargs):
    """Display the pretty (default) representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw text data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('text/plain', objs, **kwargs)


def display_html(*objs, **kwargs):
    """Display the HTML representation of an object.

    Note: If raw=False and the object does not have a HTML
    representation, no HTML will be shown.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw HTML data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('text/html', objs, **kwargs)


def display_markdown(*objs, **kwargs):
    """Displays the Markdown representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw markdown data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """

    _display_mimetype('text/markdown', objs, **kwargs)


def display_svg(*objs, **kwargs):
    """Display the SVG representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw svg data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('image/svg+xml', objs, **kwargs)


def display_png(*objs, **kwargs):
    """Display the PNG representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw png data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('image/png', objs, **kwargs)


def display_jpeg(*objs, **kwargs):
    """Display the JPEG representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw JPEG data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('image/jpeg', objs, **kwargs)


def display_latex(*objs, **kwargs):
    """Display the LaTeX representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw latex data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('text/latex', objs, **kwargs)


def display_json(*objs, **kwargs):
    """Display the JSON representation of an object.

    Note that not many frontends support displaying JSON.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw json data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('application/json', objs, **kwargs)


def display_javascript(*objs, **kwargs):
    """Display the Javascript representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw javascript data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('application/javascript', objs, **kwargs)


def display_pdf(*objs, **kwargs):
    """Display the PDF representation of an object.

    Parameters
    ----------
    objs : tuple of objects
        The Python objects to display, or if raw=True raw javascript data to
        display.
    raw : bool
        Are the data objects raw data or Python objects that need to be
        formatted before display? [default: False]
    metadata : dict (optional)
        Metadata to be associated with the specific mimetype output.
    """
    _display_mimetype('application/pdf', objs, **kwargs)


    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

#-----------------------------------------------------------------------------
# Smart classes
#-----------------------------------------------------------------------------


class DisplayObject(object):
    """An object that wraps data to be displayed."""

    _read_flags = 'r'
    _show_mem_addr = False
    metadata = None

    def __init__(self, data=None, url=None, filename=None, metadata=None):
        """Create a display object given raw data.

        When this object is returned by an expression or passed to the
        display function, it will result in the data being displayed
        in the frontend. The MIME type of the data should match the
        subclasses used, so the Png subclass should be used for 'image/png'
        data. If the data is a URL, the data will first be downloaded
        and then displayed. If

        Parameters
        ----------
        data : unicode, str or bytes
            The raw data or a URL or file to load the data from
        url : unicode
            A URL to download the data from.
        filename : unicode
            Path to a local file to load the data from.
        metadata : dict
            Dict of metadata associated to be the object when displayed
        """
        if isinstance(data, (Path, PurePath)):
            data = str(data)

        if data is not None and isinstance(data, str):
            if data.startswith('http') and url is None:
                url = data
                filename = None
                data = None
            elif _safe_exists(data) and filename is None:
                url = None
                filename = data
                data = None

        self.url = url
        self.filename = filename
        # because of @data.setter methods in
        # subclasses ensure url and filename are set
        # before assigning to self.data
        self.data = data

        if metadata is not None:
            self.metadata = metadata
        elif self.metadata is None:
            self.metadata = {}

        self.reload()
        self._check_data()

    def __repr__(self):
        if not self._show_mem_addr:
            cls = self.__class__
            r = "<%s.%s object>" % (cls.__module__, cls.__name__)
        else:
            r = super(DisplayObject, self).__repr__()
        return r

    def _check_data(self):
        """Override in subclasses if there's something to check."""
        pass

    def _data_and_metadata(self):
        """shortcut for returning metadata with shape information, if defined"""
        if self.metadata:
            return self.data, deepcopy(self.metadata)
        else:
            return self.data

    def reload(self):
        """Reload the raw data from file or URL."""
        if self.filename is not None:
            with open(self.filename, self._read_flags) as f:
                self.data = f.read()
        elif self.url is not None:
            # Deferred import
            from urllib.request import urlopen
            response = urlopen(self.url)
            data = response.read()
            # extract encoding from header, if there is one:
            encoding = None
            if 'content-type' in response.headers:
                for sub in response.headers['content-type'].split(';'):
                    sub = sub.strip()
                    if sub.startswith('charset'):
                        encoding = sub.split('=')[-1].strip()
                        break
            if 'content-encoding' in response.headers:
                # TODO: do deflate?
                if 'gzip' in response.headers['content-encoding']:
                    import gzip
                    from io import BytesIO
                    with gzip.open(BytesIO(data), 'rt', encoding=encoding) as fp:
                        encoding = None
                        data = fp.read()

            # decode data, if an encoding was specified
            # We only touch self.data once since
            # subclasses such as SVG have @data.setter methods
            # that transform self.data into ... well svg.
            if encoding:
                self.data = data.decode(encoding, 'replace')
            else:
                self.data = data


class TextDisplayObject(DisplayObject):
    """Validate that display data is text"""
    def _check_data(self):
        if self.data is not None and not isinstance(self.data, str):
            raise TypeError("%s expects text, not %r" % (self.__class__.__name__, self.data))

class Pretty(TextDisplayObject):

    def _repr_pretty_(self, pp, cycle):
        return pp.text(self.data)


class HTML(TextDisplayObject):

    def __init__(self, data=None, url=None, filename=None, metadata=None):
        def warn():
            if not data:
                return False

            #
            # Avoid calling lower() on the entire data, because it could be a
            # long string and we're only interested in its beginning and end.
            #
            prefix = data[:10].lower()
            suffix = data[-10:].lower()
            return prefix.startswith("<iframe ") and suffix.endswith("</iframe>")

        if warn():
            warnings.warn("Consider using IPython.display.IFrame instead")
        super(HTML, self).__init__(data=data, url=url, filename=filename, metadata=metadata)

    def _repr_html_(self):
        return self._data_and_metadata()

    def __html__(self):
        """
        This method exists to inform other HTML-using modules (e.g. Markupsafe,
        htmltag, etc) that this object is HTML and does not need things like
        special characters (<>&) escaped.
        """
        return self._repr_html_()

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(


class Markdown(TextDisplayObject):

    def _repr_markdown_(self):
        return self._data_and_metadata()


class Math(TextDisplayObject):

    def _repr_latex_(self):
        s = r"$\displaystyle %s$" % self.data.strip('$')
        if self.metadata:
            return s, deepcopy(self.metadata)
        else:
            return s


class Latex(TextDisplayObject):

    def _repr_latex_(self):
        return self._data_and_metadata()


class SVG(DisplayObject):
    """Embed an SVG into the display.

    Note if you just want to view a svg image via a URL use `:class:Image` with
    a url=URL keyword argument.
    """

    _read_flags = 'rb'
    # wrap data in a property, which extracts the <svg> tag, discarding
    # document headers
    _data = None

    @property
    def data(self):
        return self._data

    @data.setter
    def data(self, svg):
        if svg is None:
            self._data = None
            return
        # parse into dom object
        from xml.dom import minidom
        x = minidom.parseString(svg)
        # get svg tag (should be 1)
        found_svg = x.getElementsByTagName('svg')
        if found_svg:
            svg = found_svg[0].toxml()
        else:
            # fallback on the input, trust the user
            # but this is probably an error.
            pass
        svg = cast_unicode(svg)
        self._data = svg

    def _repr_svg_(self):
        return self._data_and_metadata()

class ProgressBar(DisplayObject):
    """Progressbar supports displaying a progressbar like element
    """
    def __init__(self, total):
        """Creates a new progressbar

        Parameters
        ----------
        total : int
            maximum size of the progressbar
        """
        self.total = total
        self._progress = 0
        self.html_width = '60ex'
        self.text_width = 60
        self._display_id = hexlify(os.urandom(8)).decode('ascii')

    def __repr__(self):
        fraction = self.progress / self.total
        filled = '=' * int(fraction * self.text_width)
        rest = ' ' * (self.text_width - len(filled))
        return '[{}{}] {}/{}'.format(
            filled, rest,
            self.progress, self.total,
        )

    def _repr_html_(self):
        return "<progress style='width:{}' max='{}' value='{}'></progress>".format(
            self.html_width, self.total, self.progress)

    def display(self):
        display(self, display_id=self._display_id)

    def update(self):
        display(self, display_id=self._display_id, update=True)

    @property
    def progress(self):
        return self._progress

    @progress.setter
    def progress(self, value):
        self._progress = value
        self.update()

    def __iter__(self):
        self.display()
        self._progress = -1 # First iteration is 0
        return self

    def __next__(self):
        """Returns current value and increments display by one."""
        self.progress += 1
        if self.progress < self.total:
            return self.progress
        else:
            raise StopIteration()

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class JSON(DisplayObject):
    """JSON expects a JSON-able dict or list

    not an already-serialized JSON string.

    Scalar types (None, number, string) are not allowed, only dict or list containers.
    """
    # wrap data in a property, which warns about passing already-serialized JSON
    _data = None
    def __init__(self, data=None, url=None, filename=None, expanded=False, metadata=None, root='root', **kwargs):
        """Create a JSON display object given raw data.

        Parameters
        ----------
        data : dict or list
            JSON data to display. Not an already-serialized JSON string.
            Scalar types (None, number, string) are not allowed, only dict
            or list containers.
        url : unicode
            A URL to download the data from.
        filename : unicode
            Path to a local file to load the data from.
        expanded : boolean
            Metadata to control whether a JSON display component is expanded.
        metadata: dict
            Specify extra metadata to attach to the json display object.
        root : str
            The name of the root element of the JSON tree
        """
        self.metadata = {
            'expanded': expanded,
            'root': root,
        }
        if metadata:
            self.metadata.update(metadata)
        if kwargs:
            self.metadata.update(kwargs)
        super(JSON, self).__init__(data=data, url=url, filename=filename)

    def _check_data(self):
        if self.data is not None and not isinstance(self.data, (dict, list)):
            raise TypeError("%s expects JSONable dict or list, not %r" % (self.__class__.__name__, self.data))

    @property
    def data(self):
        return self._data

    @data.setter
    def data(self, data):
        if isinstance(data, (Path, PurePath)):
            data = str(data)

        if isinstance(data, str):
            if self.filename is None and self.url is None:
                warnings.warn("JSON expects JSONable dict or list, not JSON strings")
            data = json.loads(data)
        self._data = data

    def _data_and_metadata(self):
        return self.data, self.metadata

    def _repr_json_(self):
        return self._data_and_metadata()

_css_t = """var link = document.createElement("link");
	link.ref = "stylesheet";
	link.type = "text/css";
	link.href = "%s";
	document.head.appendChild(link);
"""

_lib_t1 = """new Promise(function(resolve, reject) {
	var script = document.createElement("script");
	script.onload = resolve;
	script.onerror = reject;
	script.src = "%s";
	document.head.appendChild(script);
}).then(() => {
"""

_lib_t2 = """
});"""

class GeoJSON(JSON):
    """GeoJSON expects JSON-able dict

    not an already-serialized JSON string.

    Scalar types (None, number, string) are not allowed, only dict containers.
    """

    def __init__(self, *args, **kwargs):
        """Create a GeoJSON display object given raw data.

        Parameters
        ----------
        data : dict or list
            VegaLite data. Not an already-serialized JSON string.
            Scalar types (None, number, string) are not allowed, only dict
            or list containers.
        url_template : string
            Leaflet TileLayer URL template: http://leafletjs.com/reference.html#url-template
        layer_options : dict
            Leaflet TileLayer options: http://leafletjs.com/reference.html#tilelayer-options
        url : unicode
            A URL to download the data from.
        filename : unicode
            Path to a local file to load the data from.
        metadata: dict
            Specify extra metadata to attach to the json display object.

        Examples
        --------

        The following will display an interactive map of Mars with a point of
        interest on frontend that do support GeoJSON display.

            >>> from IPython.display import GeoJSON

            >>> GeoJSON(data={
            ...     "type": "Feature",
            ...     "geometry": {
            ...         "type": "Point",
            ...         "coordinates": [-81.327, 296.038]
            ...     }
            ... },
            ... url_template="http://s3-eu-west-1.amazonaws.com/whereonmars.cartodb.net/{basemap_id}/{z}/{x}/{y}.png",
            ... layer_options={
            ...     "basemap_id": "celestia_mars-shaded-16k_global",
            ...     "attribution" : "Celestia/praesepe",
            ...     "minZoom" : 0,
            ...     "maxZoom" : 18,
            ... })
            <IPython.core.display.GeoJSON object>

        In the terminal IPython, you will only see the text representation of
        the GeoJSON object.

        """

        super(GeoJSON, self).__init__(*args, **kwargs)


    def _ipython_display_(self):
        bundle = {
            'application/geo+json': self.data,
            'text/plain': '<IPython.display.GeoJSON object>'
        }
        metadata = {
            'application/geo+json': self.metadata
        }
        display(bundle, metadata=metadata, raw=True)


    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class Javascript(TextDisplayObject):

    def __init__(self, data=None, url=None, filename=None, lib=None, css=None):
        """Create a Javascript display object given raw data.

        When this object is returned by an expression or passed to the
        display function, it will result in the data being displayed
        in the frontend. If the data is a URL, the data will first be
        downloaded and then displayed.

        In the Notebook, the containing element will be available as `element`,
        and jQuery will be available.  Content appended to `element` will be
        visible in the output area.

        Parameters
        ----------
        data : unicode, str or bytes
            The Javascript source code or a URL to download it from.
        url : unicode
            A URL to download the data from.
        filename : unicode
            Path to a local file to load the data from.
        lib : list or str
            A sequence of Javascript library URLs to load asynchronously before
            running the source code. The full URLs of the libraries should
            be given. A single Javascript library URL can also be given as a
            string.
        css: : list or str
            A sequence of css files to load before running the source code.
            The full URLs of the css files should be given. A single css URL
            can also be given as a string.
        """
        if isinstance(lib, str):
            lib = [lib]
        elif lib is None:
            lib = []
        if isinstance(css, str):
            css = [css]
        elif css is None:
            css = []
        if not isinstance(lib, (list,tuple)):
            raise TypeError('expected sequence, got: %r' % lib)
        if not isinstance(css, (list,tuple)):
            raise TypeError('expected sequence, got: %r' % css)
        self.lib = lib
        self.css = css
        super(Javascript, self).__init__(data=data, url=url, filename=filename)

    def _repr_javascript_(self):
        r = ''
        for c in self.css:
            r += _css_t % c
        for l in self.lib:
            r += _lib_t1 % l
        r += self.data
        r += _lib_t2*len(self.lib)
        return r

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

# constants for identifying png/jpeg data
_PNG = b'\x89PNG\r\n\x1a\n'
_JPEG = b'\xff\xd8'

def _pngxy(data):
    """read the (width, height) from a PNG header"""
    ihdr = data.index(b'IHDR')
    # next 8 bytes are width/height
    return struct.unpack('>ii', data[ihdr+4:ihdr+12])

def _jpegxy(data):
    """read the (width, height) from a JPEG header"""
    # adapted from http://www.64lines.com/jpeg-width-height

    idx = 4
    while True:
        block_size = struct.unpack('>H', data[idx:idx+2])[0]
        idx = idx + block_size
        if data[idx:idx+2] == b'\xFF\xC0':
            # found Start of Frame
            iSOF = idx
            break
        else:
            # read another block
            idx += 2

    h, w = struct.unpack('>HH', data[iSOF+5:iSOF+9])
    return w, h

def _gifxy(data):
    """read the (width, height) from a GIF header"""
    return struct.unpack('<HH', data[6:10])


class Image(DisplayObject):

    _read_flags = 'rb'
    _FMT_JPEG = u'jpeg'
    _FMT_PNG = u'png'
    _FMT_GIF = u'gif'
    _ACCEPTABLE_EMBEDDINGS = [_FMT_JPEG, _FMT_PNG, _FMT_GIF]
    _MIMETYPES = {
        _FMT_PNG: 'image/png',
        _FMT_JPEG: 'image/jpeg',
        _FMT_GIF: 'image/gif',
    }

    def __init__(self, data=None, url=None, filename=None, format=None,
                 embed=None, width=None, height=None, retina=False,
                 unconfined=False, metadata=None):
        """Create a PNG/JPEG/GIF image object given raw data.

        When this object is returned by an input cell or passed to the
        display function, it will result in the image being displayed
        in the frontend.

        Parameters
        ----------
        data : unicode, str or bytes
            The raw image data or a URL or filename to load the data from.
            This always results in embedded image data.
        url : unicode
            A URL to download the data from. If you specify `url=`,
            the image data will not be embedded unless you also specify `embed=True`.
        filename : unicode
            Path to a local file to load the data from.
            Images from a file are always embedded.
        format : unicode
            The format of the image data (png/jpeg/jpg/gif). If a filename or URL is given
            for format will be inferred from the filename extension.
        embed : bool
            Should the image data be embedded using a data URI (True) or be
            loaded using an <img> tag. Set this to True if you want the image
            to be viewable later with no internet connection in the notebook.

            Default is `True`, unless the keyword argument `url` is set, then
            default value is `False`.

            Note that QtConsole is not able to display images if `embed` is set to `False`
        width : int
            Width in pixels to which to constrain the image in html
        height : int
            Height in pixels to which to constrain the image in html
        retina : bool
            Automatically set the width and height to half of the measured
            width and height.
            This only works for embedded images because it reads the width/height
            from image data.
            For non-embedded images, you can just set the desired display width
            and height directly.
        unconfined: bool
            Set unconfined=True to disable max-width confinement of the image.
        metadata: dict
            Specify extra metadata to attach to the image.

        Examples
        --------
        # embedded image data, works in qtconsole and notebook
        # when passed positionally, the first arg can be any of raw image data,
        # a URL, or a filename from which to load image data.
        # The result is always embedding image data for inline images.
        Image('http://www.google.fr/images/srpr/logo3w.png')
        Image('/path/to/image.jpg')
        Image(b'RAW_PNG_DATA...')

        # Specifying Image(url=...) does not embed the image data,
        # it only generates `<img>` tag with a link to the source.
        # This will not work in the qtconsole or offline.
        Image(url='http://www.google.fr/images/srpr/logo3w.png')

        """
        if isinstance(data, (Path, PurePath)):
            data = str(data)

        if filename is not None:
            ext = self._find_ext(filename)
        elif url is not None:
            ext = self._find_ext(url)
        elif data is None:
            raise ValueError("No image data found. Expecting filename, url, or data.")
        elif isinstance(data, str) and (
            data.startswith('http') or _safe_exists(data)
        ):
            ext = self._find_ext(data)
        else:
            ext = None

        if format is None:
            if ext is not None:
                if ext == u'jpg' or ext == u'jpeg':
                    format = self._FMT_JPEG
                elif ext == u'png':
                    format = self._FMT_PNG
                elif ext == u'gif':
                    format = self._FMT_GIF
                else:
                    format = ext.lower()
            elif isinstance(data, bytes):
                # infer image type from image data header,
                # only if format has not been specified.
                if data[:2] == _JPEG:
                    format = self._FMT_JPEG

        # failed to detect format, default png
        if format is None:
            format = self._FMT_PNG

        if format.lower() == 'jpg':
            # jpg->jpeg
            format = self._FMT_JPEG

        self.format = format.lower()
        self.embed = embed if embed is not None else (url is None)

        if self.embed and self.format not in self._ACCEPTABLE_EMBEDDINGS:
            raise ValueError("Cannot embed the '%s' image format" % (self.format))
        if self.embed:
            self._mimetype = self._MIMETYPES.get(self.format)

        self.width = width
        self.height = height
        self.retina = retina
        self.unconfined = unconfined
        super(Image, self).__init__(data=data, url=url, filename=filename,
                metadata=metadata)

        if self.width is None and self.metadata.get('width', {}):
            self.width = metadata['width']

        if self.height is None and self.metadata.get('height', {}):
            self.height = metadata['height']

        if retina:
            self._retina_shape()


    def _retina_shape(self):
        """load pixel-doubled width and height from image data"""
        if not self.embed:
            return
        if self.format == self._FMT_PNG:
            w, h = _pngxy(self.data)
        elif self.format == self._FMT_JPEG:
            w, h = _jpegxy(self.data)
        elif self.format == self._FMT_GIF:
            w, h = _gifxy(self.data)
        else:
            # retina only supports png
            return
        self.width = w // 2
        self.height = h // 2

    def reload(self):
        """Reload the raw data from file or URL."""
        if self.embed:
            super(Image,self).reload()
            if self.retina:
                self._retina_shape()

    def _repr_html_(self):
        if not self.embed:
            width = height = klass = ''
            if self.width:
                width = ' width="%d"' % self.width
            if self.height:
                height = ' height="%d"' % self.height
            if self.unconfined:
                klass = ' class="unconfined"'
            return u'<img src="{url}"{width}{height}{klass}/>'.format(
                url=self.url,
                width=width,
                height=height,
                klass=klass,
            )

    def _repr_mimebundle_(self, include=None, exclude=None):
        """Return the image as a mimebundle

        Any new mimetype support should be implemented here.
        """
        if self.embed:
            mimetype = self._mimetype
            data, metadata = self._data_and_metadata(always_both=True)
            if metadata:
                metadata = {mimetype: metadata}
            return {mimetype: data}, metadata
        else:
            return {'text/html': self._repr_html_()}

    def _data_and_metadata(self, always_both=False):
        """shortcut for returning metadata with shape information, if defined"""
        try:
            b64_data = b2a_base64(self.data).decode('ascii')
        except TypeError:
            raise FileNotFoundError(
                "No such file or directory: '%s'" % (self.data))
        md = {}
        if self.metadata:
            md.update(self.metadata)
        if self.width:
            md['width'] = self.width
        if self.height:
            md['height'] = self.height
        if self.unconfined:
            md['unconfined'] = self.unconfined
        if md or always_both:
            return b64_data, md
        else:
            return b64_data

    def _repr_png_(self):
        if self.embed and self.format == self._FMT_PNG:
            return self._data_and_metadata()

    def _repr_jpeg_(self):
        if self.embed and self.format == self._FMT_JPEG:
            return self._data_and_metadata()

    def _find_ext(self, s):
        base, ext = splitext(s)

        if not ext:
            return base

        # `splitext` includes leading period, so we skip it
        return ext[1:].lower()

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class Video(DisplayObject):

    def __init__(self, data=None, url=None, filename=None, embed=False,
                 mimetype=None, width=None, height=None, html_attributes="controls"):
        """Create a video object given raw data or an URL.

        When this object is returned by an input cell or passed to the
        display function, it will result in the video being displayed
        in the frontend.

        Parameters
        ----------
        data : unicode, str or bytes
            The raw video data or a URL or filename to load the data from.
            Raw data will require passing `embed=True`.
        url : unicode
            A URL for the video. If you specify `url=`,
            the image data will not be embedded.
        filename : unicode
            Path to a local file containing the video.
            Will be interpreted as a local URL unless `embed=True`.
        embed : bool
            Should the video be embedded using a data URI (True) or be
            loaded using a <video> tag (False).

            Since videos are large, embedding them should be avoided, if possible.
            You must confirm embedding as your intention by passing `embed=True`.

            Local files can be displayed with URLs without embedding the content, via::

                Video('./video.mp4')

        mimetype: unicode
            Specify the mimetype for embedded videos.
            Default will be guessed from file extension, if available.
        width : int
            Width in pixels to which to constrain the video in HTML.
            If not supplied, defaults to the width of the video.
        height : int
            Height in pixels to which to constrain the video in html.
            If not supplied, defaults to the height of the video.
        html_attributes : str
            Attributes for the HTML `<video>` block.
            Default: `"controls"` to get video controls.
            Other examples: `"controls muted"` for muted video with controls,
            `"loop autoplay"` for looping autoplaying video without controls.

        Examples
        --------

        ::

            Video('https://archive.org/download/Sita_Sings_the_Blues/Sita_Sings_the_Blues_small.mp4')
            Video('path/to/video.mp4')
            Video('path/to/video.mp4', embed=True)
            Video('path/to/video.mp4', embed=True, html_attributes="controls muted autoplay")
            Video(b'raw-videodata', embed=True)
        """
        if isinstance(data, (Path, PurePath)):
            data = str(data)

        if url is None and isinstance(data, str) and data.startswith(('http:', 'https:')):
            url = data
            data = None
        elif os.path.exists(data):
            filename = data
            data = None

        if data and not embed:
            msg = ''.join([
                "To embed videos, you must pass embed=True ",
                "(this may make your notebook files huge)\n",
                "Consider passing Video(url='...') ",
            ])
            raise ValueError(msg)

        self.mimetype = mimetype
        self.embed = embed
        self.width = width
        self.height = height
        self.html_attributes = html_attributes
        super(Video, self).__init__(data=data, url=url, filename=filename)

    def _repr_html_(self):
        width = height = ''
        if self.width:
            width = ' width="%d"' % self.width
        if self.height:
            height = ' height="%d"' % self.height

        # External URLs and potentially local files are not embedded into the
        # notebook output.
        if not self.embed:
            url = self.url if self.url is not None else self.filename
            output = """<video src="{0}" {1} {2} {3}>
      Your browser does not support the <code>video</code> element.
    </video>""".format(url, self.html_attributes, width, height)
            return output

        # Embedded videos are base64-encoded.
        mimetype = self.mimetype
        if self.filename is not None:
            if not mimetype:
                mimetype, _ = mimetypes.guess_type(self.filename)

            with open(self.filename, 'rb') as f:
                video = f.read()
        else:
            video = self.data
        if isinstance(video, str):
            # unicode input is already b64-encoded
            b64_video = video
        else:
            b64_video = b2a_base64(video).decode('ascii').rstrip()

        output = """<video {0} {1} {2}>
 <source src="data:{3};base64,{4}" type="{3}">
 Your browser does not support the video tag.
 </video>""".format(self.html_attributes, width, height, mimetype, b64_video)
        return output

    def reload(self):
        # TODO
        pass


    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class Audio(DisplayObject):
    """Create an audio object.

    When this object is returned by an input cell or passed to the
    display function, it will result in Audio controls being displayed
    in the frontend (only works in the notebook).

    Parameters
    ----------
    data : numpy array, list, unicode, str or bytes
        Can be one of

          * Numpy 1d array containing the desired waveform (mono)
          * Numpy 2d array containing waveforms for each channel.
            Shape=(NCHAN, NSAMPLES). For the standard channel order, see
            http://msdn.microsoft.com/en-us/library/windows/hardware/dn653308(v=vs.85).aspx
          * List of float or integer representing the waveform (mono)
          * String containing the filename
          * Bytestring containing raw PCM data or
          * URL pointing to a file on the web.

        If the array option is used, the waveform will be normalized.

        If a filename or url is used, the format support will be browser
        dependent.
    url : unicode
        A URL to download the data from.
    filename : unicode
        Path to a local file to load the data from.
    embed : boolean
        Should the audio data be embedded using a data URI (True) or should
        the original source be referenced. Set this to True if you want the
        audio to playable later with no internet connection in the notebook.

        Default is `True`, unless the keyword argument `url` is set, then
        default value is `False`.
    rate : integer
        The sampling rate of the raw data.
        Only required when data parameter is being used as an array
    autoplay : bool
        Set to True if the audio should immediately start playing.
        Default is `False`.
    normalize : bool
        Whether audio should be normalized (rescaled) to the maximum possible
        range. Default is `True`. When set to `False`, `data` must be between
        -1 and 1 (inclusive), otherwise an error is raised.
        Applies only when `data` is a list or array of samples; other types of
        audio are never normalized.

    Examples
    --------
    ::

        # Generate a sound
        import numpy as np
        framerate = 44100
        t = np.linspace(0,5,framerate*5)
        data = np.sin(2*np.pi*220*t) + np.sin(2*np.pi*224*t)
        Audio(data,rate=framerate)

        # Can also do stereo or more channels
        dataleft = np.sin(2*np.pi*220*t)
        dataright = np.sin(2*np.pi*224*t)
        Audio([dataleft, dataright],rate=framerate)

        Audio("http://www.nch.com.au/acm/8k16bitpcm.wav")  # From URL
        Audio(url="http://www.w3schools.com/html/horse.ogg")

        Audio('/path/to/sound.wav')  # From file
        Audio(filename='/path/to/sound.ogg')

        Audio(b'RAW_WAV_DATA..)  # From bytes
        Audio(data=b'RAW_WAV_DATA..)

    See Also
    --------

    See also the ``Audio`` widgets form the ``ipywidget`` package for more flexibility and options.

    """
    _read_flags = 'rb'

    def __init__(self, data=None, filename=None, url=None, embed=None, rate=None, autoplay=False, normalize=True, *,
                 element_id=None):
        if filename is None and url is None and data is None:
            raise ValueError("No audio data found. Expecting filename, url, or data.")
        if embed is False and url is None:
            raise ValueError("No url found. Expecting url when embed=False")

        if url is not None and embed is not True:
            self.embed = False
        else:
            self.embed = True
        self.autoplay = autoplay
        self.element_id = element_id
        super(Audio, self).__init__(data=data, url=url, filename=filename)

        if self.data is not None and not isinstance(self.data, bytes):
            if rate is None:
                raise ValueError("rate must be specified when data is a numpy array or list of audio samples.")
            self.data = Audio._make_wav(data, rate, normalize)

    def reload(self):
        """Reload the raw data from file or URL."""
        import mimetypes
        if self.embed:
            super(Audio, self).reload()

        if self.filename is not None:
            self.mimetype = mimetypes.guess_type(self.filename)[0]
        elif self.url is not None:
            self.mimetype = mimetypes.guess_type(self.url)[0]
        else:
            self.mimetype = "audio/wav"

    @staticmethod
    def _make_wav(data, rate, normalize):
        """ Transform a numpy array to a PCM bytestring """
        from io import BytesIO
        import wave

        try:
            scaled, nchan = Audio._validate_and_normalize_with_numpy(data, normalize)
        except ImportError:
            scaled, nchan = Audio._validate_and_normalize_without_numpy(data, normalize)

        fp = BytesIO()
        waveobj = wave.open(fp,mode='wb')
        waveobj.setnchannels(nchan)
        waveobj.setframerate(rate)
        waveobj.setsampwidth(2)
        waveobj.setcomptype('NONE','NONE')
        waveobj.writeframes(scaled)
        val = fp.getvalue()
        waveobj.close()

        return val

    @staticmethod
    def _validate_and_normalize_with_numpy(data, normalize):
        import numpy as np

        data = np.array(data, dtype=float)
        if len(data.shape) == 1:
            nchan = 1
        elif len(data.shape) == 2:
            # In wave files,channels are interleaved. E.g.,
            # "L1R1L2R2..." for stereo. See
            # http://msdn.microsoft.com/en-us/library/windows/hardware/dn653308(v=vs.85).aspx
            # for channel ordering
            nchan = data.shape[0]
            data = data.T.ravel()
        else:
            raise ValueError('Array audio input must be a 1D or 2D array')

        max_abs_value = np.max(np.abs(data))
        normalization_factor = Audio._get_normalization_factor(max_abs_value, normalize)
        scaled = data / normalization_factor * 32767
        return scaled.astype('<h').tostring(), nchan


    @staticmethod
    def _validate_and_normalize_without_numpy(data, normalize):
        import array
        import sys

        data = array.array('f', data)

        try:
            max_abs_value = float(max([abs(x) for x in data]))
        except TypeError:
            raise TypeError('Only lists of mono audio are '
                'supported if numpy is not installed')

        normalization_factor = Audio._get_normalization_factor(max_abs_value, normalize)
        scaled = array.array('h', [int(x / normalization_factor * 32767) for x in data])
        if sys.byteorder == 'big':
            scaled.byteswap()
        nchan = 1
        return scaled.tobytes(), nchan

    @staticmethod
    def _get_normalization_factor(max_abs_value, normalize):
        if not normalize and max_abs_value > 1:
            raise ValueError('Audio data must be between -1 and 1 when normalize=False.')
        return max_abs_value if normalize else 1

    def _data_and_metadata(self):
        """shortcut for returning metadata with url information, if defined"""
        md = {}
        if self.url:
            md['url'] = self.url
        if md:
            return self.data, md
        else:
            return self.data

    def _repr_html_(self):
        src = """
                <audio {element_id} controls="controls" {autoplay}>
                    <source src="{src}" type="{type}" />
                    Your browser does not support the audio element.
                </audio>
              """
        return src.format(src=self.src_attr(), type=self.mimetype, autoplay=self.autoplay_attr(),
                          element_id=self.element_id_attr())

    def src_attr(self):
        import base64
        if self.embed and (self.data is not None):
            data = base64=base64.b64encode(self.data).decode('ascii')
            return """data:{type};base64,{base64}""".format(type=self.mimetype,
                                                            base64=data)
        elif self.url is not None:
            return self.url
        else:
            return ""

    def autoplay_attr(self):
        if(self.autoplay):
            return 'autoplay="autoplay"'
        else:
            return ''

    def element_id_attr(self):
        if (self.element_id):
            return 'id="{element_id}"'.format(element_id=self.element_id)
        else:
            return ''

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class IFrame(object):
    """
    Generic class to embed an iframe in an IPython notebook
    """

    iframe = """
        <iframe
            width="{width}"
            height="{height}"
            src="{src}{params}"
            frameborder="0"
            allowfullscreen
        ></iframe>
        """

    def __init__(self, src, width, height, **kwargs):
        self.src = src
        self.width = width
        self.height = height
        self.params = kwargs

    def _repr_html_(self):
        """return the embed iframe"""
        if self.params:
            try:
                from urllib.parse import urlencode # Py 3
            except ImportError:
                from urllib import urlencode
            params = "?" + urlencode(self.params)
        else:
            params = ""
        return self.iframe.format(src=self.src,
                                  width=self.width,
                                  height=self.height,
                                  params=params)

class YouTubeVideo(IFrame):
    """Class for embedding a YouTube Video in an IPython session, based on its video id.

    e.g. to embed the video from https://www.youtube.com/watch?v=foo , you would
    do::

        vid = YouTubeVideo("foo")
        display(vid)

    To start from 30 seconds::

        vid = YouTubeVideo("abc", start=30)
        display(vid)

    To calculate seconds from time as hours, minutes, seconds use
    :class:`datetime.timedelta`::

        start=int(timedelta(hours=1, minutes=46, seconds=40).total_seconds())

    Other parameters can be provided as documented at
    https://developers.google.com/youtube/player_parameters#Parameters

    When converting the notebook using nbconvert, a jpeg representation of the video
    will be inserted in the document.
    """

    def __init__(self, id, width=400, height=300, **kwargs):
        self.id=id
        src = "https://www.youtube.com/embed/{0}".format(id)
        super(YouTubeVideo, self).__init__(src, width, height, **kwargs)

    def _repr_jpeg_(self):
        # Deferred import
        from urllib.request import urlopen

        try:
            return urlopen("https://img.youtube.com/vi/{id}/hqdefault.jpg".format(id=self.id)).read()
        except IOError:
            return None

class VimeoVideo(IFrame):
    """
    Class for embedding a Vimeo video in an IPython session, based on its video id.
    """

    def __init__(self, id, width=400, height=300, **kwargs):
        src="https://player.vimeo.com/video/{0}".format(id)
        super(VimeoVideo, self).__init__(src, width, height, **kwargs)

class ScribdDocument(IFrame):
    """
    Class for embedding a Scribd document in an IPython session

    Use the start_page params to specify a starting point in the document
    Use the view_mode params to specify display type one off scroll | slideshow | book

    e.g to Display Wes' foundational paper about PANDAS in book mode from page 3

    ScribdDocument(71048089, width=800, height=400, start_page=3, view_mode="book")
    """

    def __init__(self, id, width=400, height=300, **kwargs):
        src="https://www.scribd.com/embeds/{0}/content".format(id)
        super(ScribdDocument, self).__init__(src, width, height, **kwargs)

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class FileLink(object):
    """Class for embedding a local file link in an IPython session, based on path

    e.g. to embed a link that was generated in the IPython notebook as my/data.txt

    you would do::

        local_file = FileLink("my/data.txt")
        display(local_file)

    or in the HTML notebook, just::

        FileLink("my/data.txt")
    """

    html_link_str = "<a href='%s' target='_blank'>%s</a>"

    def __init__(self,
                 path,
                 url_prefix='',
                 result_html_prefix='',
                 result_html_suffix='<br>'):
        """
        Parameters
        ----------
        path : str
            path to the file or directory that should be formatted
        url_prefix : str
            prefix to be prepended to all files to form a working link [default:
            '']
        result_html_prefix : str
            text to append to beginning to link [default: '']
        result_html_suffix : str
            text to append at the end of link [default: '<br>']
        """
        if isdir(path):
            raise ValueError("Cannot display a directory using FileLink. "
              "Use FileLinks to display '%s'." % path)
        self.path = fsdecode(path)
        self.url_prefix = url_prefix
        self.result_html_prefix = result_html_prefix
        self.result_html_suffix = result_html_suffix

    def _format_path(self):
        fp = ''.join([self.url_prefix, html_escape(self.path)])
        return ''.join([self.result_html_prefix,
                        self.html_link_str % \
                            (fp, html_escape(self.path, quote=False)),
                        self.result_html_suffix])

    def _repr_html_(self):
        """return html link to file
        """
        if not exists(self.path):
            return ("Path (<tt>%s</tt>) doesn't exist. "
                    "It may still be in the process of "
                    "being generated, or you may have the "
                    "incorrect path." % self.path)

        return self._format_path()

    def __repr__(self):
        """return absolute path to file
        """
        return abspath(self.path)

class FileLinks(FileLink):
    """Class for embedding local file links in an IPython session, based on path

    e.g. to embed links to files that were generated in the IPython notebook
    under ``my/data``, you would do::

        local_files = FileLinks("my/data")
        display(local_files)

    or in the HTML notebook, just::

        FileLinks("my/data")
    """
    def __init__(self,
                 path,
                 url_prefix='',
                 included_suffixes=None,
                 result_html_prefix='',
                 result_html_suffix='<br>',
                 notebook_display_formatter=None,
                 terminal_display_formatter=None,
                 recursive=True):
        """
        See :class:`FileLink` for the ``path``, ``url_prefix``,
        ``result_html_prefix`` and ``result_html_suffix`` parameters.

        included_suffixes : list
          Filename suffixes to include when formatting output [default: include
          all files]

        notebook_display_formatter : function
          Used to format links for display in the notebook. See discussion of
          formatter functions below.

        terminal_display_formatter : function
          Used to format links for display in the terminal. See discussion of
          formatter functions below.

        Formatter functions must be of the form::

            f(dirname, fnames, included_suffixes)

        dirname : str
          The name of a directory
        fnames : list
          The files in that directory
        included_suffixes : list
          The file suffixes that should be included in the output (passing None
          meansto include all suffixes in the output in the built-in formatters)
        recursive : boolean
          Whether to recurse into subdirectories. Default is True.

        The function should return a list of lines that will be printed in the
        notebook (if passing notebook_display_formatter) or the terminal (if
        passing terminal_display_formatter). This function is iterated over for
        each directory in self.path. Default formatters are in place, can be
        passed here to support alternative formatting.

        """
        if isfile(path):
            raise ValueError("Cannot display a file using FileLinks. "
              "Use FileLink to display '%s'." % path)
        self.included_suffixes = included_suffixes
        # remove trailing slashes for more consistent output formatting
        path = path.rstrip('/')

        self.path = path
        self.url_prefix = url_prefix
        self.result_html_prefix = result_html_prefix
        self.result_html_suffix = result_html_suffix

        self.notebook_display_formatter = \
             notebook_display_formatter or self._get_notebook_display_formatter()
        self.terminal_display_formatter = \
             terminal_display_formatter or self._get_terminal_display_formatter()

        self.recursive = recursive

    def _get_display_formatter(self,
                               dirname_output_format,
                               fname_output_format,
                               fp_format,
                               fp_cleaner=None):
        """ generate built-in formatter function

           this is used to define both the notebook and terminal built-in
            formatters as they only differ by some wrapper text for each entry

           dirname_output_format: string to use for formatting directory
            names, dirname will be substituted for a single "%s" which
            must appear in this string
           fname_output_format: string to use for formatting file names,
            if a single "%s" appears in the string, fname will be substituted
            if two "%s" appear in the string, the path to fname will be
             substituted for the first and fname will be substituted for the
             second
           fp_format: string to use for formatting filepaths, must contain
            exactly two "%s" and the dirname will be substituted for the first
            and fname will be substituted for the second
        """
        def f(dirname, fnames, included_suffixes=None):
            result = []
            # begin by figuring out which filenames, if any,
            # are going to be displayed
            display_fnames = []
            for fname in fnames:
                if (isfile(join(dirname,fname)) and
                       (included_suffixes is None or
                        splitext(fname)[1] in included_suffixes)):
                      display_fnames.append(fname)

            if len(display_fnames) == 0:
                # if there are no filenames to display, don't print anything
                # (not even the directory name)
                pass
            else:
                # otherwise print the formatted directory name followed by
                # the formatted filenames
                dirname_output_line = dirname_output_format % dirname
                result.append(dirname_output_line)
                for fname in display_fnames:
                    fp = fp_format % (dirname,fname)
                    if fp_cleaner is not None:
                        fp = fp_cleaner(fp)
                    try:
                        # output can include both a filepath and a filename...
                        fname_output_line = fname_output_format % (fp, fname)
                    except TypeError:
                        # ... or just a single filepath
                        fname_output_line = fname_output_format % fname
                    result.append(fname_output_line)
            return result
        return f

    def _get_notebook_display_formatter(self,
                                        spacer="&nbsp;&nbsp;"):
        """ generate function to use for notebook formatting
        """
        dirname_output_format = \
         self.result_html_prefix + "%s/" + self.result_html_suffix
        fname_output_format = \
         self.result_html_prefix + spacer + self.html_link_str + self.result_html_suffix
        fp_format = self.url_prefix + '%s/%s'
        if sep == "\\":
            # Working on a platform where the path separator is "\", so
            # must convert these to "/" for generating a URI
            def fp_cleaner(fp):
                # Replace all occurrences of backslash ("\") with a forward
                # slash ("/") - this is necessary on windows when a path is
                # provided as input, but we must link to a URI
                return fp.replace('\\','/')
        else:
            fp_cleaner = None

        return self._get_display_formatter(dirname_output_format,
                                           fname_output_format,
                                           fp_format,
                                           fp_cleaner)

    def _get_terminal_display_formatter(self,
                                        spacer="  "):
        """ generate function to use for terminal formatting
        """
        dirname_output_format = "%s/"
        fname_output_format = spacer + "%s"
        fp_format = '%s/%s'

        return self._get_display_formatter(dirname_output_format,
                                           fname_output_format,
                                           fp_format)

    def _format_path(self):
        result_lines = []
        if self.recursive:
            walked_dir = list(walk(self.path))
        else:
            walked_dir = [next(walk(self.path))]
        walked_dir.sort()
        for dirname, subdirs, fnames in walked_dir:
            result_lines += self.notebook_display_formatter(dirname, fnames, self.included_suffixes)
        return '\n'.join(result_lines)

    def __repr__(self):
        """return newline-separated absolute paths
        """
        result_lines = []
        if self.recursive:
            walked_dir = list(walk(self.path))
        else:
            walked_dir = [next(walk(self.path))]
        walked_dir.sort()
        for dirname, subdirs, fnames in walked_dir:
            result_lines += self.terminal_display_formatter(dirname, fnames, self.included_suffixes)
        return '\n'.join(result_lines)

    )"), display_module.attr("__dict__"));
    exec(py::str(R"(

class Code(TextDisplayObject):
    """Display syntax-highlighted source code.

    This uses Pygments to highlight the code for HTML and Latex output.

    Parameters
    ----------
    data : str
        The code as a string
    url : str
        A URL to fetch the code from
    filename : str
        A local filename to load the code from
    language : str
        The short name of a Pygments lexer to use for highlighting.
        If not specified, it will guess the lexer based on the filename
        or the code. Available lexers: http://pygments.org/docs/lexers/
    """
    def __init__(self, data=None, url=None, filename=None, language=None):
        self.language = language
        super().__init__(data=data, url=url, filename=filename)

    def _get_lexer(self):
        if self.language:
            from pygments.lexers import get_lexer_by_name
            return get_lexer_by_name(self.language)
        elif self.filename:
            from pygments.lexers import get_lexer_for_filename
            return get_lexer_for_filename(self.filename)
        else:
            from pygments.lexers import guess_lexer
            return guess_lexer(self.data)

    def __repr__(self):
        return self.data

    def _repr_html_(self):
        from pygments import highlight
        from pygments.formatters import HtmlFormatter
        fmt = HtmlFormatter()
        style = '<style>{}</style>'.format(fmt.get_style_defs('.output_html'))
        return style + highlight(self.data, self._get_lexer(), fmt)

    def _repr_latex_(self):
        from pygments import highlight
        from pygments.formatters import LatexFormatter
        return highlight(self.data, self._get_lexer(), LatexFormatter())

        )"), display_module.attr("__dict__"));

        return display_module;
    }

    py::module get_display_module()
    {
        static py::module display_module = get_display_module_impl();
        return display_module;
    }
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
