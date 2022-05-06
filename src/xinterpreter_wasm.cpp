/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay,         *
* Wolf Vollprecht and Thorsten Beier                                       *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "xeus/xinterpreter.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11/functional.h"

#include "xeus-python/xinterpreter_wasm.hpp"

namespace py = pybind11;

namespace xpyt
{

    wasm_interpreter::wasm_interpreter(bool redirect_output_enabled /*=true*/, bool redirect_display_enabled /*=true*/)
        : m_redirect_display_enabled{redirect_display_enabled}
    {
        xeus::register_interpreter(this);
        if (redirect_output_enabled)
        {
            redirect_output();
        }
    }

    wasm_interpreter::~wasm_interpreter()
    {
    }

    void wasm_interpreter::instanciate_ipython_shell()
    {
        m_ipython_shell_app = py::module::import("xeus_python_shell_lite").attr("LiteXPythonShellApp")();
    }

}
