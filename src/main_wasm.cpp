/***************************************************************************
* Copyright (c) 2021, Thorsten Beier                                       *                                                       *
* Copyright (c) 2021, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <memory>

#include <emscripten/bind.h>

#include "xeus/xembind.hpp"

#include "xeus-python/xinterpreter.hpp"


EMSCRIPTEN_BINDINGS(my_module) {
    xeus::export_core();

    using interpreter_type = xpyt::interpreter;
    xeus::export_kernel<interpreter_type>("xkernel");
}
