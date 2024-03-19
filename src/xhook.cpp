/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef UVW_AS_LIB
#define UVW_AS_LIB
#include <uvw.hpp>
#endif

#include "xeus-python/xhook.hpp"

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    void hook::pre_hook()
    {
        if (!p_acquire)
        {
            p_acquire = new py::gil_scoped_acquire();
        }
    }

    void hook::post_hook()
    {
        delete p_acquire;
        p_acquire = nullptr;
    }

    void hook::run(std::shared_ptr<uvw::loop> /* loop */)
    {
        py::gil_scoped_acquire acquire;
        py::module asyncio = py::module::import("asyncio");
        py::object loop = asyncio.attr("get_event_loop")();
        loop.attr("run_forever")();
    }
}
