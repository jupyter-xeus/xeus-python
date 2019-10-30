/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XEUS_PYTHON_HOME_HPP
#define XEUS_PYTHON_HOME_HPP

#include <cstring>
#include <string>

#include "pybind11/pybind11.h"

#include "xeus_python_config.hpp"

namespace xpyt
{
    XEUS_PYTHON_API
    const char* get_pythonhome();

    inline void set_pythonhome()
    {
#ifdef _WIN32
        return;
#endif
        static const char* ph = get_pythonhome();
#if PY_MAJOR_VERSION == 2
        Py_SetPythonHome(const_cast<char*>(ph));
#else
        // The same could be achieved with Py_SetPythonHome(T"" XEUS_PYTHON_HOME)
        // but wide string literals are not properly relocated by the conda package
        // manager.
        static const std::wstring wstr(ph, ph + std::strlen(ph));;
        Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
#endif
    }
}
#endif

