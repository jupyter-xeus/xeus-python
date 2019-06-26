/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>
#include <thread>

// This must be included BEFORE pybind
// otherwise it fails to build on Windows
// because of the redefinition of snprintf
#include "nlohmann/json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "xeus/xmiddleware.hpp"

#include "xdebugger.hpp"

namespace nl = nlohmann;
namespace py = pybind11;

using namespace pybind11::literals;

namespace xpyt
{
    debugger::debugger(zmq::context_t& context,
                       const xeus::xconfiguration&)
        : m_ptvsd_client(context, xeus::get_socket_linger())
        , m_ptvsd_socket(context, zmq::socket_type::dealer)
        , m_is_started(false)
    {
        m_ptvsd_socket.setsockopt(ZMQ_LINGER, xeus::get_socket_linger());
    }

    nl::json debugger::process_request_impl(const nl::json& message)
    {
        nl::json reply;

        if(message["command"] == "attach")
        {
            start();
        }

        if(m_is_started)
        {
            std::string content = message.dump();
            size_t content_length = content.length();
            std::string buffer = xptvsd_client::HEADER
                               + std::to_string(content_length)
                               + xptvsd_client::SEPARATOR
                               + content;
            zmq::message_t raw_message(buffer.c_str(), buffer.length());
            m_ptvsd_socket.send(raw_message);

            zmq::message_t raw_reply;
            m_ptvsd_socket.recv(&raw_message);

            reply = nl::json::parse(raw_reply.data<const char>());
        }

        if(message["command"] == "disconnect")
        {
            stop();
        }

        return reply;
    }

    void debugger::start()
    {
        // TODO: should be read from configuration
        std::string host = "127.0.0.1";
        int ptvsd_port = 5678;

        // PTVSD has to be started in the main thread
        std::string ptvsd_end_point = "tcp://" + host + ':' + std::to_string(ptvsd_port);
        {
            py::gil_scoped_acquire acquire;
            py::module ptvsd = py::module::import("ptvsd");
            ptvsd.attr("enable_attach")(py::make_tuple(host, ptvsd_port), "log_dir"_a="xpython_debug_logs");
        }

        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string publisher_end_point = xeus::get_publisher_end_point();

        m_ptvsd_socket.bind(controller_end_point);

        std::thread client(&xptvsd_client::start_debugger,
                           &m_ptvsd_client,
                           ptvsd_end_point,
                           publisher_end_point,
                           controller_end_point);
        client.detach();
        zmq::message_t ack;
        m_ptvsd_socket.recv(&ack);
        m_is_started = true;
    }

    void debugger::stop()
    {
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        m_ptvsd_socket.unbind(controller_end_point);
        m_is_started = false;
    }
    
    std::unique_ptr<xeus::xdebugger> make_python_debugger(zmq::context_t& context,
                                                          const xeus::xconfiguration& config)
    {
        return std::unique_ptr<xeus::xdebugger>(new debugger(context, config));
    }
}

