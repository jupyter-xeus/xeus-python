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

#include "xeus/xinterpreter.hpp"
#include "xeus/xmiddleware.hpp"

#include "xdebugger.hpp"

namespace nl = nlohmann;
namespace py = pybind11;

using namespace pybind11::literals;

namespace xpyt
{
    debugger::debugger(zmq::context_t& context,
                       const xeus::xconfiguration& config,
                       const std::string& user_name,
                       const std::string& session_id)
        : m_ptvsd_client(context, config, xeus::get_socket_linger(), user_name, session_id)
        , m_ptvsd_socket(context, zmq::socket_type::req)
        , m_ptvsd_header(context, zmq::socket_type::req)
        , m_is_started(false)
    {
        m_ptvsd_socket.setsockopt(ZMQ_LINGER, xeus::get_socket_linger());
        m_ptvsd_header.setsockopt(ZMQ_LINGER, xeus::get_socket_linger());
    }

    nl::json debugger::process_request_impl(const nl::json& header,
                                            const nl::json& message)
    {
        nl::json reply = nl::json::object();

        if(message["command"] == "initialize")
        {
            start();
            std::cout << "XEUS-PYTHON: the debugger has started" << std::endl;
        }

        if(m_is_started)
        {
            std::string header_buffer = header.dump();
            zmq::message_t raw_header(header_buffer.c_str(), header_buffer.length());
            m_ptvsd_header.send(raw_header);
            // client responds with ACK message
            m_ptvsd_header.recv(&raw_header);

            std::string content = message.dump();
            size_t content_length = content.length();
            std::string buffer = xptvsd_client::HEADER
                               + std::to_string(content_length)
                               + xptvsd_client::SEPARATOR
                               + content;
            zmq::message_t raw_message(buffer.c_str(), buffer.length());
            m_ptvsd_socket.send(raw_message);

            zmq::message_t raw_reply;
            m_ptvsd_socket.recv(&raw_reply);

            reply = nl::json::parse(std::string(raw_reply.data<const char>(), raw_reply.size()));
       }

        if(message["command"] == "disconnect")
        {
            stop();
            std::cout << "XEUS-PYTHON: the debugger has stopped" << std::endl;
        }

        return reply;
    }

    void debugger::start()
    {
        // TODO: should be read from configuration
        std::string host = "127.0.0.1";
        int ptvsd_port = 5678;

        // PTVSD has to be started in the main thread
        std::string code = "import ptvsd\nptvsd.enable_attach((\'" + host + "\'," + std::to_string(ptvsd_port)
                         + "), log_dir=\'xpython_debug_logs\')";
        nl::json json_code;
        json_code["code"] = code;
        nl::json rep = xdebugger::get_control_messenger().send_to_shell(json_code);

        /*{
            py::gil_scoped_acquire acquire;
            py::module ptvsd = py::module::import("ptvsd");
            ptvsd.attr("enable_attach")(py::make_tuple(host, ptvsd_port), "log_dir"_a="xpython_debug_logs");
        }*/

        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        std::string publisher_end_point = xeus::get_publisher_end_point();

        m_ptvsd_socket.bind(controller_end_point);
        m_ptvsd_header.bind(controller_header_end_point);

        std::string ptvsd_end_point = "tcp://" + host + ':' + std::to_string(ptvsd_port);
        std::thread client(&xptvsd_client::start_debugger,
                           &m_ptvsd_client,
                           ptvsd_end_point,
                           publisher_end_point,
                           controller_end_point,
                           controller_header_end_point);
        client.detach();
        
        m_ptvsd_socket.send(zmq::message_t("REQ", 3));
        zmq::message_t ack;
        m_ptvsd_socket.recv(&ack);

        m_is_started = true;
    }

    void debugger::stop()
    {
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        m_ptvsd_socket.unbind(controller_end_point);
        m_ptvsd_header.unbind(controller_header_end_point);
        m_is_started = false;
    }
    
    std::unique_ptr<xeus::xdebugger> make_python_debugger(zmq::context_t& context,
                                                          const xeus::xconfiguration& config,
                                                          const std::string& user_name,
                                                          const std::string& session_id)
    {
        return std::unique_ptr<xeus::xdebugger>(new debugger(context, config, user_name, session_id));
    }
}

