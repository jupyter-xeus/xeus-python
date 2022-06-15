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
        m_release_gil_at_startup = false;
    }

    wasm_interpreter::~wasm_interpreter()
    {
    }

    void wasm_interpreter::configure_impl()
    {
        interpreter::configure_impl();

        py::gil_scoped_acquire acquire;
        py::module sys = py::module::import("pyjs");

        py::exec(R"""(
import asyncio
import traceback
import pyjs

def _install_exception_handler():
    def exception_handler(event_loop,context):
        exception = context['exception']
        traceback.print_exception(exception)
    asyncio.get_event_loop().set_exception_handler(exception_handler)
_install_exception_handler()
del _install_exception_handler

def _patch_asyncio_runner():
    from IPython.core.async_helpers import _AsyncIORunner
    def __call__(self, coro):

        task = asyncio.ensure_future(coro)

        def py_callback(resolve, reject):

            def done_cb(f):
                r = f.result()
                pyjs.js.console.log("resolving")
                if not r.success:
                    resolve()
                    raise r.error_in_exec
                else:
                    resolve()
            task.add_done_callback(done_cb)

        raw_js_py_callback = pyjs.JsValue(py_callback) 
        js_py_callback = raw_js_py_callback['__usafe_void_val_val__'].bind(raw_js_py_callback)
        js_promise = pyjs.js.Promise.new(js_py_callback)


        pyjs.js.globalThis.toplevel_promise = js_promise
        pyjs.js.globalThis.toplevel_promise_py_proxy = raw_js_py_callback
        return task
    _AsyncIORunner.__call__ = __call__
_patch_asyncio_runner()
del _patch_asyncio_runner
)""");

    }

}
