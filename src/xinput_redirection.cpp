/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <stdexcept>
#include <string>

#include "xutils.hpp"
#include "xinput_redirection.hpp"

#include "pybind11/pybind11.h"


namespace py = pybind11;

namespace xpyt
{
    input_redirection::input_redirection(bool allow_stdin)
    {
        py::module xeus_python = py::module::import("xeus_python_extension");

        // Forward input()
        py::module builtins = py::module::import(XPYT_BUILTINS);
        m_sys_input = builtins.attr("input");
        builtins.attr("input") = allow_stdin ? xeus_python.attr("input")
                                             : xeus_python.attr("notimplemented");

#if PY_MAJOR_VERSION == 2
        // Forward raw_input()
        m_sys_raw_input = builtins.attr("raw_input");
        builtins.attr("raw_input") = allow_stdin ? xeus_python.attr("raw_input")
                                                 : xeus_python.attr("notimplemented");
#endif

        // Forward getpass()
        py::module getpass = py::module::import("getpass");
        m_sys_getpass = getpass.attr("getpass");
        getpass.attr("getpass") = allow_stdin ? xeus_python.attr("getpass")
                                              : xeus_python.attr("notimplemented");
    }

    input_redirection::~input_redirection()
    {
        // Restore input()
        py::module builtins = py::module::import(XPYT_BUILTINS);
        builtins.attr("input") = m_sys_input;

#if PY_MAJOR_VERSION == 2
        // Restore raw_input()
        builtins.attr("raw_input") = m_sys_raw_input;
#endif

        // Restore getpass()
        py::module getpass = py::module::import("getpass");
        getpass.attr("getpass") = m_sys_getpass;
    }
}
