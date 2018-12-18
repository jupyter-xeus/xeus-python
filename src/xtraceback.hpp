/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
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

namespace py = pybind11;

namespace xpyt
{
    struct xerror
    {
        std::string m_ename;
        std::string m_evalue;
        std::vector<std::string> m_traceback;
    };

    xerror extract_error(py::error_already_set& error, const std::vector<std::string>& inputs);
}

#endif
