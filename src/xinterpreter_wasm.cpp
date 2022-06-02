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

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xinterpreter_wasm.hpp"

namespace py = pybind11;

namespace xpyt
{

    wasm_interpreter::wasm_interpreter()
        : interpreter(true, true)
    {
    }

    wasm_interpreter::~wasm_interpreter()
    {
    }

}
