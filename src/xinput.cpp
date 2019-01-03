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

#include "xeus/xinterpreter.hpp"
#include "xeus/xinput.hpp"

#include "pybind11/functional.h"
#include "pybind11/pybind11.h"

#include "xinput.hpp"
#include "xutils.hpp"

namespace py = pybind11;

namespace xpyt
{
    std::string cpp_input(const std::string& prompt)
    {
        return xeus::blocking_input_request(prompt, false);
    }

    std::string cpp_getpass(const std::string& prompt)
    {
        return xeus::blocking_input_request(prompt, true);
    }

    void notimplemented(const std::string&)
    {
        throw std::runtime_error("This frontend does not support input requests");
    }

    input_redirection::input_redirection(bool allow_stdin)
    {
        // Forward input()
        py::module builtins = py::module::import(XPYT_BUILTINS);
        m_sys_input = builtins.attr("input");
        builtins.attr("input") = allow_stdin ? py::cpp_function(&cpp_input, py::arg("prompt") = "")
                                             : py::cpp_function(&notimplemented, py::arg("prompt") = "");

#if PY_MAJOR_VERSION == 2
        // Forward raw_input()
        m_sys_raw_input = builtins.attr("raw_input");
        builtins.attr("raw_input") = allow_stdin ? py::cpp_function(&cpp_input, py::arg("prompt") = "")
                                                 : py::cpp_function(&notimplemented, py::arg("prompt") = "");
#endif

        // Forward getpass()
        py::module getpass = py::module::import("getpass");
        m_sys_getpass = getpass.attr("getpass");
        getpass.attr("getpass") = allow_stdin ? py::cpp_function(&cpp_getpass, py::arg("prompt") = "")
                                              : py::cpp_function(&notimplemented, py::arg("prompt") = "");
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
