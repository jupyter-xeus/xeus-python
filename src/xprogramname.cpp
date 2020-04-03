/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>

#include "pybind11/pybind11.h"

#include "xeus-python/xeus_python_config.hpp"

#include "xprogramname.hpp"

// DEBUG
#include <iostream>

namespace py = pybind11;

namespace xpyt
{
    void set_programname()
    {
#if defined(XPYTHON_PROGRAM_NAME)
        static const std::string programname = XPYT_STRINGIFY(XPYTHON_PROGRAM_NAME);
        std::cout <<  "Program Name: " << programname << std::endl; 
#if PY_MAJOR_VERSION == 2
        Py_SetProgramName(const_cast<char*>(programname.c_str()));
#else
        static const std::wstring wstr(programname.cbegin(), programname.cend());;
        Py_SetProgramName(const_cast<wchar_t*>(wstr.c_str()));
#endif
#endif
    }
}
