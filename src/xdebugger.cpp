#include <string>
#include <thread>

#include "xeus/xmiddleware.hpp"

#include "xdebugger.hpp"

namespace nl = nlohmann;

namespace xpyt
{
    debugger::debugger(zmq::context_t& context,
                       const xeus::xconfiguration&)
        : m_ptvsd_client(context, xeus::get_socket_linger())
        , m_ptvsd_socket(context, zmq::socket_type::req)
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
        int port = 5678;
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string publisher_end_point = xeus::get_publisher_end_point();

        m_ptvsd_socket.bind(controller_end_point);

        std::thread client(&xptvsd_client::start_debugger,
                           &m_ptvsd_client,
                           host,
                           port,
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

