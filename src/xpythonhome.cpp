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
#include <string>

#include <array>
#include <cstdio>
#include <string>
#include <fstream>
#include <memory>
#include <stdexcept>



#include "xeus-python/xeus_python_config.hpp"

#include "xpythonhome.hpp"
#include "xpaths.hpp"

#ifdef _WIN32
#define XPYT_POPEN _popen
#define XPYT_PCLOSE _pclose
#else
#define XPYT_POPEN popen
#define XPYT_PCLOSE pclose
#endif


namespace xpyt
{
    std::string exec2(const std::string& command)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&XPYT_PCLOSE)> pipe(XPYT_POPEN(command.c_str(), "r"), XPYT_PCLOSE);
        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }
        return result;
    }

    std::string get_host_python2()
    {
        std::ifstream host_file(prefix_path() + "/share/xeus-python/host");
        std::string host_python((std::istreambuf_iterator<char>(host_file)),
                                (std::istreambuf_iterator<char>()));
        host_python.pop_back(); // remove newline
        return host_python;
    }

    void set_pythonhome()
    {
#ifdef XPYT_DYNAMIC_SYSPATH
        // Called before Py_Initialize
        static std::string pythonhome = exec2(get_host_python2() + " -c \"import sys; print(sys.prefix)\"");
        pythonhome.pop_back(); // remove newline
#else
    // The XEUS_PYTHONHOME_RELPATH compile-time definition can be used.
    // to specify the PYTHONHOME location as a relative path to the PREFIX.
    #if defined(XEUS_PYTHONHOME_RELPATH)
        static const std::string pythonhome = prefix_path() + XPYT_STRINGIFY(XEUS_PYTHONHOME_RELPATH);
    #else
        static const std::string pythonhome = prefix_path();
    #endif
#endif

#if PY_MAJOR_VERSION == 2
        Py_SetPythonHome(const_cast<char*>(pythonhome.c_str()));
#else
        static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
        Py_SetPythonHome(const_cast<wchar_t*>(wstr.c_str()));
#endif
    }
}
