/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "pybind11/pybind11.h"

#include <iostream>
#include <regex>
#include <string>

#include "xeus-python/xeus_python_config.hpp"

#include "xsyspath.hpp"
#include "xpaths.hpp"

namespace xpyt
{
    void set_syspath()
    {
// The XEUS_PYTHON_SYSPATH compile-time definition can be used.
// to specify the syspath locations as relative paths to the PREFIX.
#if defined(XEUS_PYTHON_SYSPATH)
        static const std::string syspath = std::regex_replace(
            XPYT_STRINGIFY(XEUS_PYTHON_SYSPATH),
            std::regex("\\{PREFIX\\}"),
            prefix_path());
#if PY_MAJOR_VERSION == 2
        Py_SetPath(const_cast<char*>(syspath.c_str()));
#else
        static const std::wstring wstr(syspath.cbegin(), syspath.cend());;
        Py_SetPath(const_cast<wchar_t*>(wstr.c_str()));
#endif
#endif
    }
}
