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

def exception_handler(event_loop,context):
    exception = context['exception']
    traceback.print_exception(exception)
asyncio.get_event_loop().set_exception_handler(exception_handler)


from IPython.core.async_helpers import _AsyncIORunner

def on_done(f):
  
    r = f.result()
    err = r.error_in_exec
    if not r.success:
        raise r.error_in_exec
    
def __call__(self, coro):
    t = asyncio.ensure_future(coro)
    t.add_done_callback(on_done)
    return t
_AsyncIORunner.__call__ = __call__
)""");

    }

}
