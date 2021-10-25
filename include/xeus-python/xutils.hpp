/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/


#ifndef XPYT_UTILS_HPP
#define XPYT_UTILS_HPP

#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"

#include "xeus_python_config.hpp"

namespace py = pybind11;
namespace nl = nlohmann;


namespace xpyt
{
    XEUS_PYTHON_API XPYT_FORCE_PYBIND11_EXPORT
    bool is_pyobject_true(const py::object& obj);

    XEUS_PYTHON_API bool holding_gil();

    XEUS_PYTHON_API
    std::string extract_parameter(std::string param, int argc, char* argv[]);

    XEUS_PYTHON_API
    bool extract_option(std::string short_opt, std::string long_opt, int argc, char* argv[]);

#define XPYT_HOLDING_GIL(func)           \
    if (holding_gil())                   \
    {                                    \
        func;                            \
    }                                    \
    else                                 \
    {                                    \
        py::gil_scoped_acquire acquire;  \
        func;                            \
    }

    XEUS_PYTHON_API XPYT_FORCE_PYBIND11_EXPORT
    void exec(const py::object& code, const py::object& scope = py::globals());

    XEUS_PYTHON_API XPYT_FORCE_PYBIND11_EXPORT
    py::object eval(const py::object& code, const py::object& scope = py::globals());

    XEUS_PYTHON_API
    void sigsegv_handler(int sig);

    XEUS_PYTHON_API
    void sigkill_handler(int sig);

    XEUS_PYTHON_API
    bool should_print_version(int argc, char* argv[]);

    XEUS_PYTHON_API
    void print_pythonhome();
}

#endif
