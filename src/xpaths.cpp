/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <string>
#include <cstring>

#include "pybind11/pybind11.h"

#include "xtl/xsystem.hpp"

#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xpaths.hpp"

namespace xpyt
{
    std::string get_python_prefix()
    {
        // ------------------------------------------------------------------
        // If the PYTHONHOME environment variable is defined, use that.
        // ------------------------------------------------------------------
        const char* pythonhome_environment = std::getenv("PYTHONHOME");
        if (pythonhome_environment != nullptr && std::strlen(pythonhome_environment) != 0)
        {
            static const std::string pythonhome = pythonhome_environment;
            return pythonhome;
        }
        // ------------------------------------------------------------------
        // Otherwise, set the PYTHONHOME to the prefix path
        // ------------------------------------------------------------------
        else
        {
            // The XEUS_PYTHONHOME_RELPATH compile-time definition can be used.
            // to specify the PYTHONHOME location as a relative path to the PREFIX.
#if defined(XEUS_PYTHONHOME_RELPATH)
            static const std::string pythonhome = xtl::prefix_path() + XPYT_STRINGIFY(XEUS_PYTHONHOME_RELPATH);
#elif defined(XEUS_PYTHONHOME_ABSPATH)
            static const std::string pythonhome = XPYT_STRINGIFY(XEUS_PYTHONHOME_ABSPATH);
#else
            static const std::string pythonhome = xtl::prefix_path();
#endif
            return pythonhome;
        }
    }

    std::string get_python_path()
    {
        std::string python_prefix = get_python_prefix();
#ifdef _WIN32
        if (python_prefix.back() != '\\')
        {
            python_prefix += '\\';
        }
        return python_prefix + "python.exe";
#else
        if (python_prefix.back() != '/')
        {
            python_prefix += '/';
        }
        return python_prefix + "bin/python";
#endif
    }

    void set_pythonhome()
    {
        static const std::string pythonhome = get_python_prefix();
        static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
        Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
    }
}
