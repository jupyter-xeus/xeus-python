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
#include "xptvsd_client.hpp"

#include <iostream>
#include <thread>
#include <chrono>

namespace nl = nlohmann;

namespace xpyt
{
    xptvsd_client::xptvsd_client(zmq::context_t& context,
                                 const xeus::xconfiguration& config,
                                 int socket_linger,
                                 const xdap_tcp_configuration& dap_config,
                                 const event_callback& cb)
        : base_type(context, config, socket_linger, dap_config, cb)
    {
    }

    void xptvsd_client::handle_event(nl::json message)
    {
        if(message["event"] == "stopped" && message["body"]["reason"] == "step")
        {
            int thread_id = message["body"]["threadId"];
            int seq = message["seq"];
            nl::json frames = get_stack_frames(thread_id, seq);
            if(frames.size() == 1 && frames[0]["source"]["path"]=="<string>")
            {
                wait_next(thread_id, seq);
            }
            else
            {
                forward_event(std::move(message));
            }
        }
        else
        {
            forward_event(std::move(message));
        }
    }

    nl::json xptvsd_client::get_stack_frames(int thread_id, int seq)
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

    void xptvsd_client::wait_next(int thread_id, int seq)
    {
        nl::json request = {
            {"type", "request"},
            {"seq", seq},
            {"command", "next"},
            {"arguments", {
                {"threadId", thread_id}
            }}
        };

        send_dap_request(std::move(request));
        
        wait_for_message([thread_id](const nl::json& message)
        {
            return message["type"] == "event" && message["event"] == "continued" && message["body"]["threadId"] == thread_id;
        });

        wait_for_message([](const nl::json& message)
        {
            return message["type"] == "response" && message["command"] == "next";
        });
    }
}

