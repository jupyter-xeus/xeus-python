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

#include <cstdlib>
#include <string>

#include "xeus-python/xeus_python_config.hpp"

#include "xpythonhome.hpp"
#include "xpaths.hpp"

namespace xpyt
{
    void set_pythonhome()
    {
        // ------------------------------------------------------------------
        // If the PYTHONHOME environment variable is defined, use that.
        // ------------------------------------------------------------------
        const char* pythonhome_environment = std::getenv("PYTHONHOME");
        if (pythonhome_environment != nullptr)
        {
            static const std::string pythonhome = pythonhome_environment;
#if PY_MAJOR_VERSION == 2
            Py_SetPythonHome(const_cast<char*>(pythonhome.c_str()));
#else
            static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
            Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
#endif
        }
        // ------------------------------------------------------------------
        // Otherwise, set the PYTHONHOME to the prefix path
        // ------------------------------------------------------------------
        else
        {
            // The XEUS_PYTHONHOME_RELPATH compile-time definition can be used.
            // to specify the PYTHONHOME location as a relative path to the PREFIX.
#if defined(XEUS_PYTHONHOME_RELPATH)
            static const std::string pythonhome = prefix_path() + XPYT_STRINGIFY(XEUS_PYTHONHOME_RELPATH);
#else
            static const std::string pythonhome = prefix_path();
#endif
#if PY_MAJOR_VERSION == 2
            Py_SetPythonHome(const_cast<char*>(pythonhome.c_str()));
#else
            static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
            Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
#endif
        }
    }
}
