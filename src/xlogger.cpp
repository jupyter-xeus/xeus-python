/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"
#include "pybind11/functional.h"

#include "xlogger.hpp"

namespace py = pybind11;

namespace xpyt
{
    xlogger::xlogger()
    {
    }

    xlogger::~xlogger()
    {
    }

    void xlogger::add_logger(logger_function_type logger)
    {
        m_loggers.push_back(logger);
    }

    void xlogger::write(const std::string& message)
    {
        for (auto it = m_loggers.begin(); it != m_loggers.end(); ++it)
        {
            it->operator()(message);
        }
    }

    PYBIND11_EMBEDDED_MODULE(xeus_python_logger, m) {
        py::class_<xlogger>(m, "XPythonLogger")
            .def(py::init<>())
            .def("add_logger", &xlogger::add_logger)
            .def("write", &xlogger::write);
    }
}
