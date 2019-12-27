/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "xeus/xinterpreter.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

#include "xlinecache.hpp"
#include "xutils.hpp"

namespace py = pybind11;

namespace xpyt
{
    /******************************
     * xcheckcache implementation *
     ******************************/

    void xcheckcache(const py::str& filename = py::none())
    {
        py::module linecache = py::module::import("linecache");

        linecache.attr("_checkcache_orig")(filename);
        linecache.attr("cache").attr("update")(linecache.attr("xcache"));
    }

    /*******************************
     * xupdatecache implementation *
     *******************************/

    void xupdatecache(const py::str& code, const py::str& filename)
    {
        py::module time = py::module::import("time");
        py::module linecache = py::module::import("linecache");

        py::tuple entry = py::make_tuple(py::len(code), time.attr("time")(), code.attr("splitlines")(true), filename);

        linecache.attr("cache")[filename] = entry;
        linecache.attr("xcache")[filename] = entry;
    }

    /********************
     * linecache module *
     ********************/

    py::module get_linecache_module_impl()
    {
        py::module linecache_module("linecache");

        exec(py::str(R"(
from linecache import getline, getlines, updatecache, cache, clearcache, lazycache
from linecache import checkcache as _checkcache_orig

xcache = {}
            )"), linecache_module.attr("__dict__"));

        linecache_module.def("checkcache",
            xcheckcache,
            py::arg("filename") = py::none()
        );

        linecache_module.def("xupdatecache",
            xupdatecache,
            py::arg("code"),
            py::arg("filename")
        );

        return linecache_module;
    }

    py::module get_linecache_module()
    {
        static py::module linecache_module = get_linecache_module_impl();
        return linecache_module;
    }
}
