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

#include "pybind11/embed.h"
#include "pybind11/functional.h"
#include "pybind11/pybind11.h"

#include "xinput.hpp"
#include "xutils.hpp"

namespace py = pybind11;

namespace xpyt
{
    std::string input_request(const std::string& prompt, bool b)
    {
        auto& interpreter = xeus::get_interpreter();

        // Register the input handler
        std::string value;
        interpreter.register_input_handler([&value](const std::string& v) {
            value = v;
        });

        // Send the input request.
        interpreter.input_request(prompt, b);

        // Remove input handler;
        interpreter.register_input_handler(nullptr);

        // TODO: Handle EOF
        // if value == '\x04':
        //    raise EOFError

        return value;
    }

    // Free functions for intput, raw_input and getpass
    std::string input(const std::string& prompt)
    {
        return input_request(prompt, false);
    }

    std::string getpass(const std::string& prompt)
    {
        return input_request(prompt, true);
    }

    void notimplemented(const std::string&)
    {
        throw std::runtime_error("This frontend does not support input requests");
    }

    PYBIND11_EMBEDDED_MODULE(xeus_python_input, m)
    {
        m.def("input", input, py::arg("prompt") = "")
         .def("getpass", getpass, py::arg("prompt") = "")
         .def("notimplemented", notimplemented, py::arg("prompt") = "");

#if PY_MAJOR_VERSION == 2
        m.def("raw_input", input, py::arg("prompt") = "");
#endif
    }

    // Implementation of input_redirection

    input_redirection::input_redirection(bool allow_stdin)
    {
        py::module xeus_python_input = py::module::import("xeus_python_input");

        // Forward input()
        py::module builtins = py::module::import(XPYT_BUILTINS);
        m_sys_input = builtins.attr("input");
        builtins.attr("input") = allow_stdin ? xeus_python_input.attr("input")
                                             : xeus_python_input.attr("notimplemented");

#if PY_MAJOR_VERSION == 2
        // Forward raw_input()
        m_sys_raw_input = builtins.attr("raw_input");
        builtins.attr("raw_input") = allow_stdin ? xeus_python_input.attr("raw_input")
                                                 : xeus_python_input.attr("notimplemented");
#endif

        // Forward getpass()
        py::module getpass = py::module::import("getpass");
        m_sys_getpass = getpass.attr("getpass");
        getpass.attr("getpass") = allow_stdin ? xeus_python_input.attr("getpass")
                                              : xeus_python_input.attr("notimplemented");
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
