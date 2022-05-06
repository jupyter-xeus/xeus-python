/***************************************************************************
* Copyright (c) 2021, QuantStack
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_INTERPRETER_WASM_HPP
#define XPYT_INTERPRETER_WASM_HPP

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "xeus/xinterpreter.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"

#include "xeus_python_config.hpp"

namespace py = pybind11;

namespace xpyt
{
    class XEUS_PYTHON_API wasm_interpreter : public xeus::xinterpreter
    {
    public:

        using gil_scoped_release_ptr = std::unique_ptr<py::gil_scoped_release>;

        wasm_interpreter();
        virtual ~wasm_interpreter();

    protected:

        void instanciate_ipython_shell();

        py::scoped_interpreter m_interpreter;
    };

}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif
