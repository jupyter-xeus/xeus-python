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
#include <memory>
#include <mutex>
#include <set>

#include "nlohmann/json.hpp"
#include "pybind11/pybind11.h"
#include "xeus-zmq/xdebugger_base.hpp"
#include "xeus-zmq/xthread.hpp"
#include "xeus_python_config.hpp"

namespace py = pybind11;

namespace xpyt
{

    class xdebugpy_client;

    class XEUS_PYTHON_API debugger : public xeus::xdebugger_base
    {
    public:

        using base_type = xeus::xdebugger_base;

        debugger(
                py::dict globals,
                xeus::xcontext& context,
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
        nl::json copy_to_globals_request(const nl::json& message);
        nl::json modules(const nl::json& message);

        nl::json variables_request_impl(const nl::json& message) override;

        bool start_debugpy();
        bool start() override;
        void stop() override;
        xeus::xdebugger_info get_debugger_info() const override;
        std::string get_cell_temporary_file(const std::string& code) const override;

        
        py::dict m_global_dict;

        std::unique_ptr<xdebugpy_client> p_debugpy_client;
        std::string m_debugpy_host;
        std::string m_debugpy_port;
        nl::json m_debugger_config;
        py::object m_pydebugger;
        xeus::xthread m_client_runner;
        bool m_copy_to_globals_available;
    };

    XEUS_PYTHON_API
    std::unique_ptr<xeus::xdebugger> make_python_debugger(py::dict globals,
                                                          xeus::xcontext& context,
                                                          const xeus::xconfiguration& config,
                                                          const std::string& user_name,
                                                          const std::string& session_id,
                                                          const nl::json& debugger_config);
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif
