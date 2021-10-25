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
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/eval.h"

#include "xtl/xhash.hpp"

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

    std::string extract_parameter(std::string param, int argc, char* argv[])
    {
        std::string res = "";
        for (int i = 0; i < argc; ++i)
        {
            if ((std::string(argv[i]) == param) && (i + 1 < argc))
            {
                res = argv[i + 1];
                for (int j = i; j < argc - 2; ++j)
                {
                    argv[j] = argv[j + 2];
                }
                argc -= 2;
                break;
            }
        }
        return res;
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
#ifdef __GNUC__
        void* array[10];

        // get void*'s for all entries on the stack
        size_t size = backtrace(array, 10);

        // print out all the frames to stderr
        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
        exit(1);
    }

    void sigkill_handler(int /*sig*/)
    {
        exit(0);
    }

    bool should_print_version(int argc, char* argv[])
    {
        for (int i = 0; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--version")
            {
                return true;
            }
        }
        return false;
    }

    void print_pythonhome()
    {
        std::setlocale(LC_ALL, "en_US.utf8");
        wchar_t* ph = Py_GetPythonHome();

        char mbstr[1024];
        std::wcstombs(mbstr, ph, 1024);

        std::clog << "PYTHONHOME set to " << mbstr << std::endl;
    }
}
