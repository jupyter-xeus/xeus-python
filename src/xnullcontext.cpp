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

#include "xeus-python/xutils.hpp"

#include "xnullcontext.hpp"
#include "xinternal_utils.hpp"

namespace py = pybind11;

namespace xpyt
{
    /**********************
     * nullcontext module *
     **********************/

    py::module get_nullcontext_module_impl()
    {
        py::module nullcontext_module = create_module("xeus_nullcontext");
        exec(py::str(R"(
from contextlib import AbstractContextManager
class nullcontext(AbstractContextManager):
    """nullcontext for contextlib.nullcontext"""
    def __init__(self, enter_result=None):
        self.enter_result = enter_result
    def __enter__(self):
        return self.enter_result
    def __exit__(self, *excinfo):
        pass
        )"), nullcontext_module.attr("__dict__"));
        return nullcontext_module;
    }

    py::module get_nullcontext_module()
    {
        static py::module nullcontext_module = get_nullcontext_module_impl();
        return nullcontext_module;
    }
}
