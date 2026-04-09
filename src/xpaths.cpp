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
#include <filesystem>

#include "pybind11/pybind11.h"

#include "xeus/xsystem.hpp"

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
            static const std::string pythonhome = xeus::prefix_path() + XPYT_STRINGIFY(XEUS_PYTHONHOME_RELPATH);
#elif defined(XEUS_PYTHONHOME_ABSPATH)
            static const std::string pythonhome = XPYT_STRINGIFY(XEUS_PYTHONHOME_ABSPATH);
#else
            using namespace std::filesystem;
#   ifdef _WIN32
            // python is located in the root dir of the env on Windows, not the prefix path which is in env-root/Library/
            static const std::string pythonhome = [] {
                    // makes sure std::filesystem knows the string we pass is indeed UTF-8 (by convention)
                    // and not some other encoding (as often expected on Windows)
                    const path prefix_path_u8(xeus::prefix_path(), std::locale("en_US.UTF-8"));
                    // we need the parent path of the prefix path
                    const auto python_dir_u8 = canonical(prefix_path_u8).parent_path().u8string(); 
                    // TOOD: in C++20, `python_dir_u8` will have changed type to `std::u8string`, a conversion would be needed here.
                    return python_dir_u8;
                }();
#   else
            static const std::string pythonhome = canonical(xeus::prefix_path()).u8string();
#   endif
#endif
            return pythonhome;
        }
    }

    std::string get_python_path()
    {
        const char* python_exe_environment = std::getenv("PYTHON_EXECUTABLE");
        if (python_exe_environment != nullptr && std::strlen(python_exe_environment) != 0)
        {
            static const std::string python_exe_path = python_exe_environment;
            return python_exe_path;
        }

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

    [[deprecated]]
    void set_pythonhome()
    {
        // static const std::string pythonhome = get_python_prefix();
        // static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
        // Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
    }

}
