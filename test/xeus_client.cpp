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

#include "xeus_client.hpp"
#include "xeus/xguid.hpp"
#include "xeus/xmessage.hpp"
#include "xeus-zmq/xmiddleware.hpp"

using namespace std::chrono_literals;

/*************************************
 * xeus_logger_client implementation *
 *************************************/

xeus_logger_client::xeus_logger_client(xeus::xcontext& context,
                                       const std::string& user_name,
                                       const xeus::xconfiguration& config,
                                       const std::string& file_name)
    : m_user_name(user_name)
    , m_file_name(file_name)
    , m_session_id(xeus::new_xguid())
    , p_client(xeus::make_xclient_zmq(context, config))
{
    std::ofstream out(m_file_name);
    out << "STARTING CLIENT" << std::endl;

    p_client->connect();
    register_kernel_status_listener();
}

xeus_logger_client::~xeus_logger_client()
{
}

void xeus_logger_client::register_kernel_status_listener()
{
    p_client->register_kernel_status_listener([this](bool status) {
        handle_kernel_status_message(status);
    });
}

void xeus_logger_client::handle_kernel_status_message(bool status)
{
    kernel_dead = status;
}

void xeus_logger_client::send_on_shell(const std::string& msg_type, nl::json content)
{
    nl::json header = xeus::make_header(msg_type, m_user_name, m_session_id);

    nl::json result;
    result["header"] = header;
    result["parent_header"] = nl::json::object();
    result["metadata"] = nl::json::object();
    result["content"] = content;
    log_message(result);

    xeus::xmessage msg(xeus::xmessage::guid_list(),
                       std::move(header),
                       nl::json::object(),
                       nl::json::object(),
                       std::move(content),
                       xeus::buffer_sequence());
    
    p_client->send_on_shell(std::move(msg));
}

void xeus_logger_client::send_on_control(const std::string& msg_type, nl::json content)
{
    nl::json header = xeus::make_header(msg_type, m_user_name, m_session_id);

    nl::json result;
    result["header"] = header;
    result["parent_header"] = nl::json::object();
    result["metadata"] = nl::json::object();
    result["content"] = content;
    log_message(result);

    xeus::xmessage msg(xeus::xmessage::guid_list(),
                       std::move(header),
                       nl::json::object(),
                       nl::json::object(),
                       std::move(content),
                       xeus::buffer_sequence());
    
    p_client->send_on_control(std::move(msg));
}

nl::json xeus_logger_client::receive_on_shell()
{
    std::optional<xeus::xmessage> msg_opt = p_client->receive_on_shell();
    nl::json result = nl::json::object();

    if (msg_opt.has_value())
    {
        xeus::xmessage msg = std::move(*msg_opt);
        result["header"] = msg.header();
        result["parent_header"] = msg.parent_header();
        result["metadata"] = msg.metadata();
        result["content"] = msg.content();
    }
    log_message(result);

    return result;
}

nl::json xeus_logger_client::receive_on_control()
{
    std::optional<xeus::xmessage> msg_opt = p_client->receive_on_control();
    nl::json result = nl::json::object();

    if (msg_opt.has_value())
    {
        xeus::xmessage msg = std::move(*msg_opt);
        result["header"] = msg.header();
        result["parent_header"] = msg.parent_header();
        result["metadata"] = msg.metadata();
        result["content"] = msg.content();
    }
    log_message(result);

    return result;
}

std::size_t xeus_logger_client::iopub_queue_size() const
{
    return p_client->iopub_queue_size();
}

nl::json xeus_logger_client::pop_iopub_message()
{
    std::optional<xeus::xpub_message> msg_opt = p_client->pop_iopub_message();
    nl::json result = nl::json::object();

    if (msg_opt.has_value())
    {
        xeus::xpub_message msg = std::move(*msg_opt);
        result["topic"] = msg.topic();
        result["header"] = msg.header();
        result["parent_header"] = msg.parent_header();
        result["metadata"] = msg.metadata();
        result["content"] = msg.content();
    }

    return result;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return msg;
}

void xeus_logger_client::start()
{
    p_client->start();
}

void xeus_logger_client::stop_channels()
{
    p_client->stop_channels();
}

void xeus_logger_client::log_message(nl::json msg)
{
    std::lock_guard<std::mutex> guard(m_file_mutex);
    std::ofstream out(m_file_name, std::ios_base::app);
    out << msg.dump(4) << std::endl;
}