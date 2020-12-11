/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

#if defined(__linux__)
#  include <unistd.h>
#endif
#if defined(_WIN32)
#  if defined(NOMINMAX)
#    include <windows.h>
#  else
#    define NOMINMAX
#    include <windows.h>
#    undef NOMINMAX
#  endif
#endif
#ifdef __APPLE__
#  include <cstdint>
#  include <mach-o/dyld.h>
#endif
#if defined(__sun)
#  include <stdlib.h>
#endif

#include "pybind11/pybind11.h"

#include "xeus-python/xeus_python_config.hpp"

#include "xpaths.hpp"

namespace xpyt
{
    std::string executable_path()
    {
        std::string path;
        char buffer[1024];
        std::memset(buffer, '\0', sizeof(buffer));
#if defined(__linux__)
        if (readlink("/proc/self/exe", buffer, sizeof(buffer)) != -1)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined (_WIN32)
        if (GetModuleFileName(nullptr, buffer, sizeof(buffer)) != 0)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined (__APPLE__)
        std::uint32_t size = sizeof(buffer);
        if(_NSGetExecutablePath(buffer, &size) == 0)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined (__FreeBSD__)
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        if (sysctl(mib, 4, buffer, sizeof(buffer), NULL, 0) != -1)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined(__sun)
        path = getexecname();
#endif
        return path;
    }

    std::string prefix_path()
    {
        std::string path = executable_path();
#if defined (_WIN32)
        char separator = '\\';
#else
        char separator = '/';
#endif

        std::string bin_folder = path.substr(0, path.find_last_of(separator));
        std::string prefix = bin_folder.substr(0, bin_folder.find_last_of(separator)) + separator;

        return prefix;
    }

    std::string get_python_prefix()
    {
        // ------------------------------------------------------------------
        // If the PYTHONHOME environment variable is defined, use that.
        // ------------------------------------------------------------------
        const char* pythonhome_environment = std::getenv("PYTHONHOME");
        if (pythonhome_environment != nullptr)
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
            static const std::string pythonhome = prefix_path() + XPYT_STRINGIFY(XEUS_PYTHONHOME_RELPATH);
#else
            static const std::string pythonhome = prefix_path();
#endif
            return pythonhome;
        }
    }

    std::string get_python_path()
    {
#ifdef _WIN32
        return get_python_prefix() + "python.exe";
#else
        return get_python_prefix() + "bin/python";
#endif
    }

    void set_pythonhome()
    {
        static const std::string pythonhome = get_python_prefix();
        static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
        Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
    }
}
