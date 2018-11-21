/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>

#include "xeus/xinterpreter.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"
#include "pybind11/functional.h"

#include "xlogger.hpp"

namespace py = pybind11;

namespace xpyt
{
    xlogger::xlogger(std::string pipe_name) : m_pipe_name(pipe_name)
    {
    }

    xlogger::~xlogger()
    {
    }

    void xlogger::write(const std::string& message)
    {
        xeus::get_interpreter().publish_stream(m_pipe_name, message);
    }

    PYBIND11_EMBEDDED_MODULE(xeus_python_logger, m) {
        py::class_<xlogger>(m, "XPythonLogger")
            .def(py::init<std::string>())
            .def("write", &xlogger::write);
    }
}
