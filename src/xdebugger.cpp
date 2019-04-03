/***************************************************************************
* Copyright (c) 2019, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>

#include "zmq_addon.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace pybind11::literals;

namespace xpyt
{
    std::string get_end_point(const std::string& ip, const std::size_t& port)
    {
        return "tcp://" + ip + ':' + std::to_string(port);
    }

    /*************************
     * xdebugger declaration *
     *************************/

    class xdebugger
    {
    public:

        xdebugger();
        virtual ~xdebugger();

    private:

        void init_server();
        void init_client();

        zmq::context_t m_context;
        std::string m_host;
        std::size_t m_server_port;
        zmq::socket_t m_client_socket;
    };

    /****************************
     * xdebugger implementation *
     ****************************/

    xdebugger::xdebugger()
        : m_host("127.0.0.1")
        , m_server_port(5678) // Hardcoded for now, we need a way to find an available port
        , m_client_socket(m_context, zmq::socket_type::stream)
    {
        init_server();
        init_client();
    }

    xdebugger::~xdebugger()
    {
        // TODO: Send stop event to ptvsd
    }

    void xdebugger::init_server()
    {
        py::module ptvsd = py::module::import("ptvsd");

        ptvsd.attr("enable_attach")(py::make_tuple(m_host, m_server_port));
    }

    void xdebugger::init_client()
    {
        m_client_socket.connect(get_end_point(m_host, m_server_port));
    }

    /*******************
     * debugger module *
     *******************/

    py::module get_debugger_module_impl()
    {
        py::module debugger_module = py::module("kernel");

        py::class_<xdebugger>(debugger_module, "Debugger")
            .def(py::init<>());

        debugger_module.def("debugging", [debugger_module]() {
            auto debugger_start_callback = [debugger_module] (xeus::xcomm&& comm, const xeus::xmessage& msg) {
                py::module sys = py::module::import("sys");

                // TODO remove this print
                py::print("Debugger Comm opened, starting debugger", "file"_a=sys.attr("__stdout__"));

                py::object debugger = py::globals()["xdebugger"];

                if (debugger.is_none())
                {
                    debugger = debugger_module.attr("Debugger")();
                    py::globals()["xdebugger"] = debugger;
                }
                else
                {
                    py::print("Debugger Comm already opened", "file"_a=sys.attr("__stderr__"));
                    return;
                }

                // On message, forward it to ptvsd and send back the response to the client?
                // comm.on_message();

                // On Comm close, stop the communication?
                // comm.on_close();
            };

            xeus::get_interpreter().comm_manager().register_comm_target(
                "jupyter.debugger", debugger_start_callback
            );
        });

        return debugger_module;
    }

    py::module get_debugger_module()
    {
        static py::module debugger_module = get_debugger_module_impl();
        return debugger_module;
    }
}
