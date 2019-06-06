/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_DEBUGGER_HPP
#define XPYT_DEBUGGER_HPP

#include "nlohmann/json.hpp"
#include "xeus/xdebugger.hpp"

namespace xpyt
{
    class debugger : public xeus::xdebugger
    {
    public:

        debugger(zmq::context_t& context,
                 const xeus::xconfiguration& config);

        virtual ~debugger() = default;

    private:

        virtual nl::json process_request_impl(const nl::json& message);
    };

    std::unique_ptr<xeus::xdebugger> make_python_debugger(zmq::context_t& context,
                                                          const xeus::xconfiguration& config);
}

#endif
