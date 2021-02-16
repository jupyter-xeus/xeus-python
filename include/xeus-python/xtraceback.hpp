/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_TRACEBACK_HPP
#define XPYT_TRACEBACK_HPP

#include <vector>
#include <string>

#include "pybind11/pybind11.h"

#include "xeus_python_config.hpp"

namespace py = pybind11;

namespace xpyt
{
    struct XEUS_PYTHON_API xerror
    {
        std::string m_ename;
        std::string m_evalue;
        std::vector<std::string> m_traceback;
    };

    XEUS_PYTHON_API py::module get_traceback_module();

    XEUS_PYTHON_API void register_filename_mapping(const std::string& filename, int execution_count);

    XEUS_PYTHON_API XPYT_FORCE_PYBIND11_EXPORT
    xerror extract_error(py::error_already_set& error);

    XEUS_PYTHON_API XPYT_FORCE_PYBIND11_EXPORT
    xerror extract_error(const py::object& type, const py::object& value, const py::object& traceback);
}

#endif
