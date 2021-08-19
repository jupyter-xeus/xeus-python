/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>
#include <utility>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/eval.h"

#include "xeus-python/xutils.hpp"

#include "xkernel.hpp"
#include "xinternal_utils.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    /***********************
     * xkernel declaration *
     **********************/

    struct xkernel
    {
        xkernel() = default;

        py::dict get_parent();

        py::object m_comm_manager;
    };

    /**************************
     * xkernel implementation *
     **************************/

    py::dict xkernel::get_parent()
    {
        return py::dict(py::arg("header")=xeus::get_interpreter().parent_header().get<py::object>());
    }

    /*****************
     * kernel module *
     *****************/

    py::module get_kernel_module_impl()
    {
        py::module kernel_module = create_module("kernel");

        py::class_<xkernel>(kernel_module, "XKernel")
            .def(py::init<>())
            .def("get_parent", &xkernel::get_parent)
            .def_property_readonly("_parent_header", &xkernel::get_parent)
            .def_readwrite("comm_manager", &xkernel::m_comm_manager);

        return kernel_module;
    }

    py::module get_kernel_module()
    {
        static py::module kernel_module = get_kernel_module_impl();
        return kernel_module;
    }
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
