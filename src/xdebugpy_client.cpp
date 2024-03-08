/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "zmq_addon.hpp"
#include "nlohmann/json.hpp"
#include "xeus/xmessage.hpp"
#include "xeus/xeus_context.hpp"
#include "xdebugpy_client.hpp"

#include <thread>
#include <chrono>

namespace nl = nlohmann;

namespace xpyt
{
    xdebugpy_client::xdebugpy_client(xeus::xcontext& context,
                                     const xeus::xconfiguration& config,
                                     int socket_linger,
                                     const xdap_tcp_configuration& dap_config,
                                     const event_callback& cb)
        : base_type(context, config, socket_linger, dap_config, cb)
    {
    }

    void xdebugpy_client::handle_event(nl::json message)
    {
        forward_event(std::move(message));
    }

    nl::json xdebugpy_client::get_stack_frames(int thread_id, int seq)
    {
        nl::json request = {
            {"type", "request"},
            {"seq", seq},
            {"command", "stackTrace"},
            {"arguments", {
                {"threadId", thread_id}
            }}
        };

        send_dap_request(std::move(request));

        nl::json reply = wait_for_message([](const nl::json& message)
        {
            return message["type"] == "response" && message["command"] == "stackTrace";
        });
        return reply["body"]["stackFrames"];
    }
}

