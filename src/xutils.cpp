/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef WIN32
#include "Windows.h"
#endif

#ifdef __GNUC__
#include <stdio.h>
#ifndef XPYT_EMSCRIPTEN_WASM_BUILD
#include <execinfo.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#endif

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/eval.h"

#include "xeus-python/xutils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    bool is_pyobject_true(const py::object& obj)
    {
        return PyObject_IsTrue(obj.ptr());
    }

    bool holding_gil()
    {
        return PyGILState_Check();
    }

    void exec(const py::object& code, const py::object& scope)
    {
        py::exec("exec(_code_, _scope_, _scope_)", py::globals(), py::dict(py::arg("_code_") = code, py::arg("_scope_") = scope));
    }

    py::object eval(const py::object& code, const py::object& scope)
    {
        return py::eval(code, scope);
    }

    bool extract_option(std::string short_opt, std::string long_opt, int argc, char* argv[])
    {
        bool res = false;
        for (int i = 0; i < argc; ++i)
        {
            if (((std::string(argv[i]) == short_opt) || (std::string(argv[i]) == long_opt)))
            {
                res = true;
                for (int j = i; j < argc - 1; ++j)
                {
                    argv[j] = argv[j + 1];
                }
                argc -= 1;
                break;
            }
        }
        return res;
    }

    void sigsegv_handler(int sig)
    {
        std::cout << "Segmentation fault (signal " << sig << "). Exiting..." << std::endl;
#if defined(__GNUC__) && !defined(XPYT_EMSCRIPTEN_WASM_BUILD)
        void* array[10];

        // get void*'s for all entries on the stack
        int size = backtrace(array, 10);

        // print out all the frames to stderr
        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
        exit(1);
    }

    void sigkill_handler(int /*sig*/)
    {
        std::cout << "Received termination signal. Exiting..." << std::endl;
        // make sure to aquire the gil before exiting
        //exit(0);
    }

    void print_pythonhome()
    {
        std::setlocale(LC_ALL, "en_US.utf8");
        // Py_GetPythonHome will return NULL if called before Py_Initialize()
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        wchar_t* ph = config.home;

        if (ph)
        {
            char mbstr[1024];
            std::wcstombs(mbstr, ph, 1024);
            std::clog << "PYTHONHOME set to " << mbstr << std::endl;
        }
        else
        {
            std::clog << "PYTHONHOME not set or not initialized." << std::endl;
        }
    }

    // Compares 2 versions and return true if version1 < version2.
    // The versions must be strings formatted as the regex: [0-9]+(\s[0-9]+)*
    bool less_than_version(std::string version1, std::string version2)
    {
        using iterator_type = std::istream_iterator<int>;
        std::istringstream iv1(version1), iv2(version2);
        return std::lexicographical_compare(
            iterator_type(iv1),
            iterator_type(),
            iterator_type(iv2),
            iterator_type()
        );
    }
}
