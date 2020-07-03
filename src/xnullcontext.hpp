/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_NULLCONTEXT_HPP
#define XPYT_NULLCONTEXT_HPP

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

namespace py = pybind11;

namespace xpyt
{
    // Provides a polyfill for Python 3.7's contextlib.nullcontext
    py::module get_nullcontext_module();
}

#endif
