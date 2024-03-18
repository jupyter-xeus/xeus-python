/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_HOOK_HPP
#define XPYT_HOOK_HPP

// pybind11 code internally forces hidden visibility on all internal code, but
// if non-hidden (and thus exported) code attempts to include a pybind type
// this warning occurs:
// 'xpyt::hook' declared with greater visibility than the type of its
// field 'xpyt::hook::p_acquire' [-Wattributes]
#ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "xeus-python/xeus_python_config.hpp"
#include "xeus-zmq/xhook_base.hpp"

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    XEUS_PYTHON_API
    class hook : public xeus::xhook_base
    {
    public:
        hook() = default;

        void pre_hook() override;
        void post_hook() override;
        void run(std::shared_ptr<uvw::loop> loop) override;

    private:
        py::gil_scoped_acquire* p_acquire{ nullptr};
    };

}

#endif
