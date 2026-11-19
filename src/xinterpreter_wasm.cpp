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
        : interpreter(py::globals(), true, true)
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
        py::module::import("pyjs");

        // Import getpass now, before JupyterLite mounts the drive as the cwd.
        // input_redirection imports it on the first execute_request. At that point
        // '' (the cwd, i.e. the drive) is first on sys.path, and getpass' failed
        // "import termios" / "import msvcrt" lookups go through the drive, where
        // every stat is a round-trip to the main thread (~90 ms on the first cell
        // on a mid-range Android tablet).
        py::module::import("getpass");
    }

    bool wasm_interpreter::use_jedi_for_completion() const
    {
        return false;
    }
}
