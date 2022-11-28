/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "zmq_addon.hpp"

#include "xeus_client.hpp"
#include "xeus/xguid.hpp"
#include "xeus/xmessage.hpp"
#include "xeus-zmq/xmiddleware.hpp"
#include "xeus-zmq/xzmq_serializer.hpp"

using namespace std::chrono_literals;

/***********************************
 * xeus-client_base implementation *
 ***********************************/

xeus_client_base::xeus_client_base(zmq::context_t& context,
                                   const std::string& user_name,
                                   const xeus::xconfiguration& config)
    : p_shell_authentication(xeus::make_xauthentication(config.m_signature_scheme, config.m_key))
    , p_control_authentication(xeus::make_xauthentication(config.m_signature_scheme, config.m_key))
    , p_iopub_authentication(xeus::make_xauthentication(config.m_signature_scheme, config.m_key))
    , m_shell(context, zmq::socket_type::dealer)
    , m_control(context, zmq::socket_type::dealer)
    , m_iopub(context, zmq::socket_type::sub)
    , m_shell_end_point("")
    , m_control_end_point("")
    , m_iopub_end_point("")
    , m_user_name(user_name)
    , m_session_id(xeus::new_xguid())
{
    m_shell_end_point = xeus::get_end_point(config.m_transport, config.m_ip, config.m_shell_port);
    m_control_end_point = xeus::get_end_point(config.m_transport, config.m_ip, config.m_control_port);
    m_iopub_end_point = xeus::get_end_point(config.m_transport, config.m_ip, config.m_iopub_port);

    m_shell.connect(m_shell_end_point);
    m_control.connect(m_control_end_point);
    m_iopub.connect(m_iopub_end_point);
}

xeus_client_base::~xeus_client_base()
{
    m_shell.disconnect(m_shell_end_point);
    m_control.disconnect(m_control_end_point);
    m_iopub.disconnect(m_iopub_end_point);
}

void xeus_client_base::send_on_shell(nl::json header,
                                     nl::json parent_header,
                                     nl::json metadata,
                                     nl::json content)
{
    send_message(std::move(header),
                 std::move(parent_header),
                 std::move(metadata),
                 std::move(content),
                 m_shell,
                 *p_shell_authentication);
}

nl::json xeus_client_base::receive_on_shell()
{
    return receive_message(m_shell, *p_shell_authentication);
}

void xeus_client_base::send_on_control(nl::json header,
                                       nl::json parent_header,
                                       nl::json metadata,
                                       nl::json content)
{
    send_message(std::move(header),
                 std::move(parent_header),
                 std::move(metadata),
                 std::move(content),
                 m_control,
                 *p_control_authentication);
}

nl::json xeus_client_base::receive_on_control()
{
    return receive_message(m_control, *p_control_authentication);
}

void xeus_client_base::subscribe_iopub(const std::string& filter)
{
    m_iopub.setsockopt(ZMQ_SUBSCRIBE, filter.c_str(), filter.length());
}

void xeus_client_base::unsubscribe_iopub(const std::string& filter)
{
    m_iopub.setsockopt(ZMQ_UNSUBSCRIBE, filter.c_str(), filter.length());
}

nl::json xeus_client_base::receive_on_iopub()
{
    zmq::multipart_t wire_msg;
    wire_msg.recv(m_iopub);
    xeus::xpub_message msg = xeus::xzmq_serializer::deserialize_iopub(wire_msg, *p_iopub_authentication);

    nl::json res =  aggregate(msg.header(),
                              msg.parent_header(),
                              msg.metadata(),
                              msg.content());
    res["topic"] = msg.topic();
    return res;
}

nl::json xeus_client_base::make_header(const std::string& msg_type) const
{
    return xeus::make_header(msg_type, m_user_name, m_session_id);
}

void xeus_client_base::send_message(nl::json header,
                                    nl::json parent_header,
                                    nl::json metadata,
                                    nl::json content,
                                    zmq::socket_t& socket,
                                    const xeus::xauthentication& auth)
{
    xeus::xmessage msg(xeus::xmessage::guid_list(),
                       std::move(header),
                       std::move(parent_header),
                       std::move(metadata),
                       std::move(content),
                       xeus::buffer_sequence());
    zmq::multipart_t wire_msg = xeus::xzmq_serializer::serialize(std::move(msg), auth);
    wire_msg.send(socket);
}

nl::json xeus_client_base::receive_message(zmq::socket_t& socket,
                                           const xeus::xauthentication& auth)
{
    zmq::multipart_t wire_msg;
    wire_msg.recv(socket);
    xeus::xmessage msg = xeus::xzmq_serializer::deserialize(wire_msg, auth);

    return aggregate(msg.header(),
                     msg.parent_header(),
                     msg.metadata(),
                     msg.content());
}

nl::json xeus_client_base::aggregate(const nl::json& header,
                                     const nl::json& parent_header,
                                     const nl::json& metadata,
                                     const nl::json& content) const
{
    nl::json result;
    result["header"] = header;
    result["parent_header"] = parent_header;
    result["metadata"] = metadata;
    result["content"] = content;
    return result;
}

/*************************************
 * xeus_logger_client implementation *
 *************************************/

xeus_logger_client::xeus_logger_client(zmq::context_t& context,
                                       const std::string& user_name,
                                       const xeus::xconfiguration& config,
                                       const std::string& file_name)
    : xeus_client_base(context, user_name, config)
    , m_file_name(file_name)
    , m_iopub_stopped(false)
{
    std::ofstream out(m_file_name);
    out << "STARTING CLIENT" << std::endl;
    base_type::subscribe_iopub("");
    std::thread iopub_thread(&xeus_logger_client::poll_iopub, this);
    iopub_thread.detach();
}

xeus_logger_client::~xeus_logger_client()
{
    while(!m_iopub_stopped)
    {
        std::this_thread::sleep_for(100ms);
    }
}

void xeus_logger_client::send_on_shell(const std::string& msg_type, nl::json content)
{
    nl::json header = base_type::make_header(msg_type);
    log_message(base_type::aggregate(header,
                                     nl::json::object(),
                                     nl::json::object(),
                                     content));
    base_type::send_on_shell(std::move(header),
                             nl::json::object(),
                             nl::json::object(),
                             std::move(content));
}

void xeus_logger_client::send_on_control(const std::string& msg_type, nl::json content)
{
    nl::json header = base_type::make_header(msg_type);
    log_message(base_type::aggregate(header,
                                     nl::json::object(),
                                     nl::json::object(),
                                     content));
    base_type::send_on_control(std::move(header),
                               nl::json::object(),
                               nl::json::object(),
                               std::move(content));
}

nl::json xeus_logger_client::receive_on_shell()
{
    nl::json msg = base_type::receive_on_shell();
    log_message(msg);
    return msg;
}

nl::json xeus_logger_client::receive_on_control()
{
    nl::json msg = base_type::receive_on_control();
    log_message(msg);
    return msg;
}

std::size_t xeus_logger_client::iopub_queue_size() const
{
    std::lock_guard<std::mutex> guard(m_queue_mutex);
    return m_message_queue.size();
}

nl::json xeus_logger_client::pop_iopub_message()
{
    std::lock_guard<std::mutex> guard(m_queue_mutex);
    nl::json res = m_message_queue.back();
    m_message_queue.pop();
    return res;
}

nl::json xeus_logger_client::wait_for_debug_event(const std::string& event)
{
    bool event_found = false;
    nl::json msg;
    while(!event_found)
    {
        std::size_t s = iopub_queue_size();
        if(s != 0)
        {
            msg = pop_iopub_message();
            if(msg["topic"] == "debug_event" && msg["content"]["event"] == event)
            {
                event_found = true;
            }
        }
        else
        {
            std::unique_lock<std::mutex> lk(m_notify_mutex);
            if(iopub_queue_size())
            {
                continue;
            }
            m_notify_cond.wait(lk);
        }
    }
    return msg;
}

void xeus_logger_client::poll_iopub()
{
    while(true)
    {
        nl::json msg = base_type::receive_on_iopub();
        {
            std::unique_lock<std::mutex> lk(m_notify_mutex);
            std::unique_lock<std::mutex> guard(m_queue_mutex);
            m_message_queue.push(msg);
            guard.unlock();
            lk.unlock();
            m_notify_cond.notify_one();
        }
        std::string topic = msg["topic"];
        std::size_t topic_size = topic.size();
        log_message(std::move(msg));
        if(topic.substr(topic_size - 8, topic_size) == "shutdown")
        {
            std::cout << "Received shutdown, exiting" << std::endl;
            break;
        }
    }
    m_iopub_stopped = true;
}

void xeus_logger_client::log_message(nl::json msg)
{
    std::lock_guard<std::mutex> guard(m_file_mutex);
    std::ofstream out(m_file_name, std::ios_base::app);
    out << msg.dump(4) << std::endl;
}
