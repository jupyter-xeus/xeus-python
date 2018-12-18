/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_INSPECT_HPP
#define XPYT_INSPECT_HPP

#include <string>

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    py::object static_inspect(const std::string& code, int cursor_pos);
    py::object static_inspect(const std::string& code);

    std::string formatted_docstring(const std::string& code, int cursor_pos);
    std::string formatted_docstring(const std::string& code);
}

#endif
