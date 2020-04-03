/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <array>
#include <cstdio>
#include <string>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "pybind11/pybind11.h"

#include "xeus-python/xeus_python_config.hpp"

#include "xsyspath.hpp"
#include "xpaths.hpp"

#ifdef _WIN32
#define XPYT_POPEN _popen
#define XPYT_PCLOSE _pclose
#else
#define XPYT_POPEN popen
#define XPYT_PCLOSE pclose
#endif


namespace py = pybind11;

namespace xpyt
{
    std::string exec(const std::string& command)
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

    std::string get_host_python()
    {
        std::ifstream host_file(prefix_path() + "/share/xeus-python/host");
        std::string host_python((std::istreambuf_iterator<char>(host_file)),
                                (std::istreambuf_iterator<char>()));
        host_python.pop_back(); // remove newline
        return host_python;
    }

    void set_syspath()
    {
        // Called before Py_Initialize
        std::string syspath = exec(get_host_python() + " -c \"import sys; print(':'.join(sys.path))\"");
        syspath.pop_back(); // remove newline
#if PY_MAJOR_VERSION == 2
        // Py_SetPath is Python 3 only
        // Py_SetPath(const_cast<char*>(syspath.c_str()));
#else
        std::wstring wstr(syspath.cbegin(), syspath.cend());;
        Py_SetPath(const_cast<wchar_t*>(wstr.c_str()));
#endif
    }
}
