/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
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
                                 const std::string& user_name,
                                 const std::string& session_id)
        : m_ptvsd_socket(context, zmq::socket_type::stream)
        , m_id_size(256)
        , m_publisher(context, zmq::socket_type::pub)
        , m_controller(context, zmq::socket_type::rep)
        , m_controller_header(context, zmq::socket_type::rep)
        , m_user_name(user_name)
        , m_session_id(session_id)
        , p_auth(xeus::make_xauthentication(config.m_signature_scheme, config.m_key))
        , m_parent_header("")
        , m_request_stop(false)
    {
        m_ptvsd_socket.setsockopt(ZMQ_LINGER, socket_linger);
        m_publisher.setsockopt(ZMQ_LINGER, socket_linger);
        m_controller.setsockopt(ZMQ_LINGER, socket_linger);
        m_controller_header.setsockopt(ZMQ_LINGER, socket_linger);
    }

    void xptvsd_client::start_debugger(std::string ptvsd_end_point,
                                       std::string publisher_end_point,
                                       std::string controller_end_point,
                                       std::string controller_header_end_point)
    {
        m_publisher.connect(publisher_end_point);
        m_controller.connect(controller_end_point);
        m_controller_header.connect(controller_header_end_point);
        
        m_ptvsd_socket.connect(ptvsd_end_point);
        m_ptvsd_socket.getsockopt(ZMQ_IDENTITY, m_socket_id, &m_id_size);

        // Tells the controller that the connection with
        // ptvsd has been established
        zmq::message_t req;
        m_controller.recv(&req);
        m_controller.send(zmq::message_t("ACK", 3));
        
        zmq::pollitem_t items[] = {
            { m_controller_header, 0, ZMQ_POLLIN, 0 },
            { m_controller, 0, ZMQ_POLLIN, 0 },
            { m_ptvsd_socket, 0, ZMQ_POLLIN, 0 }
        };
        
        m_request_stop = false;
        while(!m_request_stop)
        {
            zmq::poll(&items[0], 3, -1);

            if(items[0].revents & ZMQ_POLLIN)
            {
                handle_header_socket();
            }

            if(items[1].revents & ZMQ_POLLIN)
            {
                handle_control_socket();
            }

            if(items[2].revents & ZMQ_POLLIN)
            {
                handle_ptvsd_socket();
            }

            process_message_queue();
        }

        m_ptvsd_socket.disconnect(ptvsd_end_point);
        m_controller.disconnect(controller_end_point);
        m_controller_header.disconnect(controller_header_end_point);
        m_publisher.disconnect(publisher_end_point);
        // Reset m_request_stop for the next debugging session
        m_request_stop = false;
    }

    void xptvsd_client::process_message_queue()
    {
        while(!m_message_queue.empty())
        {
            const std::string& raw_message = m_message_queue.front();
            nl::json message = nl::json::parse(raw_message);
            // message is either an event or a response
            if(message["type"] == "event")
            {
                zmq::multipart_t wire_msg;
                nl::json header = xeus::make_header("debug_event", m_user_name, m_session_id);
                nl::json parent_header = m_parent_header.empty() ? nl::json::object() : nl::json::parse(m_parent_header);
                xeus::xpub_message msg("debug_event",
                                       std::move(header),
                                       std::move(parent_header),
                                       nl::json::object(),
                                       std::move(message),
                                       xeus::buffer_sequence());
                std::move(msg).serialize(wire_msg, *p_auth);
                wire_msg.send(m_publisher);
            }
            else
            {
                if(message["command"] == "disconnect")
                {
                    m_request_stop = true;
                }
                zmq::message_t reply(raw_message.c_str(), raw_message.size());
                m_controller.send(reply);
            }
            m_message_queue.pop();
        }
    }

    void xptvsd_client::handle_header_socket()
    {
        zmq::message_t message;
        m_controller_header.recv(&message);
        m_parent_header = std::string(message.data<const char>(), message.size());
        m_controller_header.send(zmq::message_t("ACK", 3));
    }

    void xptvsd_client::handle_ptvsd_socket()
    {
        using size_type = std::string::size_type;
        
        std::string buffer = "";
        bool messages_received = false;
        size_type header_pos = std::string::npos;
        size_type separator_pos = std::string::npos;
        size_type msg_size = 0;
        size_type msg_pos = std::string::npos;
        size_type hint = 0;

        while(!messages_received)
        {
            while(header_pos == std::string::npos)
            {
                append_tcp_message(buffer);
                header_pos = buffer.find(HEADER, hint);
            }

            separator_pos = buffer.find(SEPARATOR, header_pos + HEADER_LENGTH);
            while(separator_pos == std::string::npos)
            {
                hint = buffer.size();
                append_tcp_message(buffer);
                separator_pos = buffer.find(SEPARATOR, hint);
            }

            msg_size = std::stoull(buffer.substr(header_pos + HEADER_LENGTH, separator_pos));
            msg_pos = separator_pos + SEPARATOR_LENGTH;

            // The end of the buffer does not contain a full message
            while(buffer.size() - msg_pos < msg_size)
            {
                append_tcp_message(buffer);
            }

            // The end of the buffer contains a full message
            if(buffer.size() - msg_pos == msg_size)
            {
                m_message_queue.push(buffer.substr(msg_pos));
                messages_received = true;
            }
            else
            {
                // The end of the buffer contains a full message
                // and the beginning of a new one. We push the first
                // one in the queue, and loop again to get the next
                // one.
                m_message_queue.push(buffer.substr(msg_pos, msg_size));
                hint = msg_pos + msg_size;
                header_pos = buffer.find(HEADER, hint);
                separator_pos = std::string::npos;
            }
        }
    }

    void xptvsd_client::handle_control_socket()
    {
        zmq::message_t message;
        m_controller.recv(&message);

        // Sends a ZMQ header (required for stream socket) and forwards
        // the message
        m_ptvsd_socket.send(zmq::message_t(m_socket_id, m_id_size), ZMQ_SNDMORE);
        m_ptvsd_socket.send(message);
    }

    void xptvsd_client::append_tcp_message(std::string& buffer)
    {
        // First message is a ZMQ header that we discard
        zmq::message_t header;
        m_ptvsd_socket.recv(&header);

        zmq::message_t content;
        m_ptvsd_socket.recv(&content);

        buffer += std::string(content.data<const char>(), content.size());
    }
}

