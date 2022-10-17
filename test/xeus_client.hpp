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

#include "zmq.hpp"
#include "nlohmann/json.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus-zmq/xauthentication.hpp"

// Base class for clients, provides an API to 
// send and receive messages, but nothing more ;)

namespace nl = nlohmann;

class xeus_client_base
{
public:

    xeus_client_base(zmq::context_t& context,
                     const std::string& user_name,
                     const xeus::xconfiguration& config);

    virtual ~xeus_client_base();

    void subscribe_iopub(const std::string& filter);
    void unsubscribe_iopub(const std::string& filter);

protected:

    void send_on_shell(nl::json header,
                       nl::json parent_header,
                       nl::json metadata,
                       nl::json content);
    nl::json receive_on_shell();

    void send_on_control(nl::json header,
                         nl::json parent_header,
                         nl::json metadata,
                         nl::json content);
    nl::json receive_on_control();

    nl::json receive_on_iopub();

    nl::json make_header(const std::string& msg_type) const;
    nl::json aggregate(const nl::json& header,
                       const nl::json& parent_header,
                       const nl::json& metadata,
                       const nl::json& content) const;

private:

    void send_message(nl::json header,
                      nl::json parent_header,
                      nl::json metadata,
                      nl::json content,
                      zmq::socket_t& socket,
                      const xeus::xauthentication& auth);

    nl::json receive_message(zmq::socket_t& socket,
                             const xeus::xauthentication& auth);

    using authentication_ptr = std::unique_ptr<xeus::xauthentication>;
    authentication_ptr p_shell_authentication;
    authentication_ptr p_control_authentication;
    authentication_ptr p_iopub_authentication;

    zmq::socket_t m_shell;
    zmq::socket_t m_control;
    zmq::socket_t m_iopub;

    std::string m_shell_end_point;
    std::string m_control_end_point;
    std::string m_iopub_end_point;

    std::string m_user_name;
    std::string m_session_id;
};

// Client that logs sent and received messages.
// Runs the iopub poller in a dedicated thread and
// push messages in a queue for future usage.
class xeus_logger_client : public xeus_client_base
{
public:

    xeus_logger_client(zmq::context_t& context,
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

private:

    using base_type = xeus_client_base;

    void poll_iopub();
    void log_message(nl::json msg);

    std::string m_file_name;
    bool m_iopub_stopped;
    std::queue<nl::json> m_message_queue;
    std::mutex m_file_mutex;
    mutable std::mutex m_queue_mutex;
    std::mutex m_notify_mutex;
    std::condition_variable m_notify_cond;
};

