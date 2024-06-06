/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "nlohmann/json.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus/xmessage.hpp"
#include "xeus-zmq/xclient_zmq.hpp"

namespace nl = nlohmann;

// Client that logs sent and received messages.
// Based on xclient_zmq from xeus-zmq.

class xeus_logger_client
{
public:
    using client_ptr = std::unique_ptr<xeus::xclient_zmq>;

    xeus_logger_client(xeus::xcontext& context,
                       const std::string& user_name,
                       const xeus::xconfiguration& config,
                       const std::string& file_name);

    virtual ~xeus_logger_client();

    void send_on_shell(const std::string& msg_type, nl::json content);
    void send_on_control(const std::string& msg_type, nl::json content);

    nl::json receive_on_shell();
    nl::json receive_on_control();

    std::size_t iopub_queue_size() const;
    nl::json pop_iopub_message();

    nl::json wait_for_debug_event(const std::string& event);
    void start();
    void stop_channels();
    void log_message(nl::json msg);

    void register_kernel_status_listener();
    bool kernel_dead = false;

private:

    void handle_kernel_status_message(bool status);

    std::string m_user_name;
    std::string m_file_name;
    std::string m_session_id;
    std::mutex m_file_mutex;

    client_ptr p_client;
};