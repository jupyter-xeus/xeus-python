/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_DEBUGGER_HPP
#define XPYT_DEBUGGER_HPP

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <map>
#include <mutex>
#include <set>

#include "zmq.hpp"
#include "nlohmann/json.hpp"
#include "xeus/xeus_context.hpp"
#include "pybind11/pybind11.h"
#include "xeus-zmq/xdebugger_base.hpp"
#include "xeus_python_config.hpp"

namespace py = pybind11;

namespace xpyt
{

    class xdebugpy_client;

    class XEUS_PYTHON_API debugger : public xeus::xdebugger_base
    {
    public:

        using base_type = xeus::xdebugger_base;

        debugger(zmq::context_t& context,
                 const xeus::xconfiguration& config,
                 const std::string& user_name,
                 const std::string& session_id,
                 const nl::json& debugger_config);

        virtual ~debugger();

    private:

        nl::json inspect_variables_request(const nl::json& message);
        nl::json rich_inspect_variables_request(const nl::json& message);
        nl::json attach_request(const nl::json& message);
        nl::json configuration_done_request(const nl::json& message);

        nl::json variables_request_impl(const nl::json& message) override;

        bool start_debugpy();
        bool start(zmq::socket_t& header_socket,
                   zmq::socket_t& request_socket) override;
        void stop(zmq::socket_t& header_socket,
                  zmq::socket_t& request_socket) override;
        xeus::xdebugger_info get_debugger_info() const override;
        std::string get_cell_temporary_file(const std::string& code) const override;

        xdebugpy_client* p_debugpy_client;
        std::string m_debugpy_host;
        std::string m_debugpy_port;
        nl::json m_debugger_config;
        py::object m_pydebugger;
    };

    XEUS_PYTHON_API
    std::unique_ptr<xeus::xdebugger> make_python_debugger(xeus::xcontext& context,
                                                          const xeus::xconfiguration& config,
                                                          const std::string& user_name,
                                                          const std::string& session_id,
                                                          const nl::json& debugger_config);
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif
