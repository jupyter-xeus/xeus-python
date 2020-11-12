/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

// This must be included BEFORE pybind
// otherwise it fails to build on Windows
// because of the redefinition of snprintf
#include "nlohmann/json.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "xeus/xinterpreter.hpp"
#include "xeus/xmiddleware.hpp"
#include "xeus/xsystem.hpp"

#include "xeus-python/xdebugger.hpp"
#include "xdebugpy_client.hpp"
#include "xutils.hpp"

namespace nl = nlohmann;
namespace py = pybind11;

using namespace pybind11::literals;
using namespace std::placeholders;

namespace xpyt
{
    debugger::debugger(zmq::context_t& context,
                       const xeus::xconfiguration& config,
                       const std::string& user_name,
                       const std::string& session_id)
        : xdebugger_base(context)
        , p_debugpy_client(new xdebugpy_client(context,
                                               config,
                                               xeus::get_socket_linger(),
                                               xdap_tcp_configuration(xeus::dap_tcp_type::client,
                                                                      xeus::dap_init_type::parallel,
                                                                      user_name,
                                                                      session_id),
                                               get_event_callback()))
        , m_debugpy_host("127.0.0.1")
        , m_debugpy_port("")
    {
        m_debugpy_port = xeus::find_free_port(100, 5678, 5900);
        register_request_handler("inspectVariables", std::bind(&debugger::inspect_variables_request, this, _1), false);
        register_request_handler("attach", std::bind(&debugger::attach_request, this, _1), true);
        register_request_handler("configurationDone", std::bind(&debugger::configuration_done_request, this, _1), true);
    }

    debugger::~debugger()
    {
        delete p_debugpy_client;
        p_debugpy_client = nullptr;
    }

    nl::json debugger::inspect_variables_request(const nl::json& message)
    {
        py::gil_scoped_acquire acquire;
        py::object variables = py::globals();

        nl::json json_vars = nl::json::array();
        for (const py::handle& key : variables)
        {
            nl::json json_var = nl::json::object();
            json_var["name"] = py::str(key);
            json_var["variablesReference"] = 0;
            try
            {
                json_var["value"] = variables[key];
            }
            catch(std::exception&)
            {
                json_var["value"] = py::repr(variables[key]);
            }
            json_vars.push_back(json_var);
        }

        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]},
            {"body", {
                {"variables", json_vars}
            }}
        };

        return reply;
    }

    nl::json debugger::attach_request(const nl::json& message)
    {
        nl::json new_message = message;
        new_message["arguments"]["connect"] = {
            {"host", m_debugpy_host},
            {"port", std::stoi(m_debugpy_port)}
        };
        new_message["arguments"]["logToFile"] = true;
        return forward_message(new_message);
    }

    nl::json debugger::configuration_done_request(const nl::json& message)
    {
        int seq = message["seq"].get<int>();
        nl::json reply = {
            {"seq", seq},
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]}
        };
        return reply;
    }

    bool debugger::start_debugpy()
    {
        // Setting Program Name
        std::string executable(XEUS_PYTHON_EXECUTABLE);
        // debugpy has to be started in the main thread
        std::string code = "import debugpy\ndebugpy.configure({\'python\': \'" + executable  + "\'})\ndebugpy.listen((\'" + m_debugpy_host + "\'," + m_debugpy_port + "))";
        nl::json json_code;
        json_code["code"] = code;
        nl::json rep = xdebugger::get_control_messenger().send_to_shell(json_code);
        std::string status = rep["status"].get<std::string>();
        if(status != "ok")
        {
            std::string ename = rep["ename"].get<std::string>();
            std::string evalue = rep["evalue"].get<std::string>();
            std::vector<std::string> traceback = rep["traceback"].get<std::vector<std::string>>();
            std::clog << "Exception raised when trying to import debugpy" << std::endl;
            for(std::size_t i = 0; i < traceback.size(); ++i)
            {
                std::clog << traceback[i] << std::endl;
            }
            std::clog << ename << " - " << evalue << std::endl;
        }
        return status == "ok";
    }

    bool debugger::start(zmq::socket_t& header_socket, zmq::socket_t& request_socket)
    {
        std::string temp_dir = xeus::get_temp_directory_path();
        std::string log_dir = temp_dir + "/" + "xpython_debug_logs_" + std::to_string(xeus::get_current_pid());

        xeus::create_directory(log_dir);

        static bool debugpy_started = start_debugpy();
        if (!debugpy_started)
        {
            return false;
        }

        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        std::string publisher_end_point = xeus::get_publisher_end_point();

        request_socket.bind(controller_end_point);
        header_socket.bind(controller_header_end_point);

        std::string debugpy_end_point = "tcp://" + m_debugpy_host + ':' + m_debugpy_port;
        std::thread client(&xdap_tcp_client::start_debugger,
                           p_debugpy_client,
                           debugpy_end_point,
                           publisher_end_point,
                           controller_end_point,
                           controller_header_end_point);
        client.detach();

        request_socket.send(zmq::message_t("REQ", 3), zmq::send_flags::none);
        zmq::message_t ack;
        (void)request_socket.recv(ack);

        std::string tmp_folder =  get_tmp_prefix();
        xeus::create_directory(tmp_folder);

        return true;
    }

    void debugger::stop(zmq::socket_t& header_socket, zmq::socket_t& request_socket)
    {
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        request_socket.unbind(controller_end_point);
        header_socket.unbind(controller_header_end_point);
    }

    xeus::xdebugger_info debugger::get_debugger_info() const
    {
        return xeus::xdebugger_info(get_hash_seed(),
                                    get_tmp_prefix(),
                                    get_tmp_suffix());
    }

    std::string debugger::get_cell_temporary_file(const std::string& code) const
    {
        return get_cell_tmp_file(code);
    }
    
    std::unique_ptr<xeus::xdebugger> make_python_debugger(zmq::context_t& context,
                                                          const xeus::xconfiguration& config,
                                                          const std::string& user_name,
                                                          const std::string& session_id)
    {
        return std::unique_ptr<xeus::xdebugger>(new debugger(context, config, user_name, session_id));
    }
}

