/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"

#include "xpython_display.hpp"

namespace py = pybind11;

namespace xpyt
{
    xdisplayhook::xdisplayhook() : m_execution_count(0)
    {
    }

    xdisplayhook::~xdisplayhook()
    {
    }

    void xdisplayhook::set_execution_count(int execution_count)
    {
        m_execution_count = execution_count;
    }

    void xdisplayhook::add_hook(hook_function_type hook)
    {
        m_hooks.push_back(hook);
    }

    void xdisplayhook::operator()(py::object obj)
    {
        for (auto it = m_hooks.begin(); it != m_hooks.end(); ++it)
        {
            it->operator()(m_execution_count, obj);
        }
    }

    PYBIND11_EMBEDDED_MODULE(xeus_python_display, m) {
        py::class_<xdisplayhook>(m, "XPythonDisplay")
            .def(py::init<>())
            .def("set_execution_count", &xdisplayhook::set_execution_count)
            .def("add_hook", &xdisplayhook::add_hook)
            .def("__call__", &xdisplayhook::operator());
    }
}
