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
#include <pybind11/embed.h>
// #include <pyjs/export_pyjs_module.hpp>
// #include <pyjs/export_js_module.hpp>

#include "xeus/xembind.hpp"
#include "xeus-python/xinterpreter_wasm.hpp"


// PYBIND11_EMBEDDED_MODULE(pyjs, m) {
//     std::cout<<"export_pyjs_module \n";
//     pyjs::export_pyjs_module(m);
//     std::cout<<"export_pyjs_module DONE\n";
// }

EMSCRIPTEN_BINDINGS(my_module) {
    xeus::export_core();
    // pyjs::export_js_module();
    using interpreter_type = xpyt::wasm_interpreter;
    xeus::export_kernel<interpreter_type>("xkernel");
}
