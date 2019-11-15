/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_DEBUGGER_HPP
#define XPYT_DEBUGGER_HPP

#include <map>
#include <mutex>
#include <set>

#include "zmq.hpp"
#include "nlohmann/json.hpp"
#include "xeus/xdebugger.hpp"
#include "xptvsd_client.hpp"

#include "xeus-python/xeus_python_config.hpp"

namespace xpyt
{

    class XEUS_PYTHON_API debugger : public xeus::xdebugger
    {
    public:

        debugger(zmq::context_t& context,
                 const xeus::xconfiguration& config,
                 const std::string& user_name,
                 const std::string& session_id);

        virtual ~debugger() = default;

    private:

        virtual nl::json process_request_impl(const nl::json& header,
                                              const nl::json& message);

        nl::json forward_message(const nl::json& message);
        nl::json dump_cell_request(const nl::json& message);
        nl::json set_breakpoints_request(const nl::json& message);
        nl::json source_request(const nl::json& message);
        nl::json debug_info_request(const nl::json& message);
        nl::json inspect_variables_request(const nl::json& message);

        void start();
        void stop();
        void handle_event(const nl::json& message);

        xptvsd_client m_ptvsd_client;
        zmq::socket_t m_ptvsd_socket;
        zmq::socket_t m_ptvsd_header;
        // PTVSD cannot be started on different ports in a same process
        // so we need to remember the port once it has be found.
        std::string m_ptvsd_port;
        using breakpoint_list_t = std::map<std::string, std::vector<nl::json>>;
        breakpoint_list_t m_breakpoint_list;
        std::set<int> m_stopped_threads;
        std::mutex m_stopped_mutex;
        bool m_is_started;

    };

    XEUS_PYTHON_API
    std::unique_ptr<xeus::xdebugger> make_python_debugger(zmq::context_t& context,
                                                          const xeus::xconfiguration& config,
                                                          const std::string& user_name,
                                                          const std::string& session_id);
}

#endif
