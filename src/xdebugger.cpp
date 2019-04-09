/***************************************************************************
* Copyright (c) 2019, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <string>
#include <thread>

#include "zmq_addon.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus/xmessage.hpp"

#include "xeus-python/xinterpreter.hpp"

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
        void client_run();

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

        ptvsd.attr("enable_attach")(py::make_tuple(m_host, m_server_port), "log_dir"_a="xpython_debug_logs");
    }

    void xdebugger::init_client()
    {
        std::thread client_thread(&xdebugger::client_run, this);
        client_thread.detach();
    }

    void xdebugger::client_run()
    {
        m_client_socket.connect(get_end_point(m_host, m_server_port));

        while (true) // TODO Find a stop condition (ptvsd exit message?)
        {
            zmq::pollitem_t items[] = { { m_client_socket, 0, ZMQ_POLLIN, 0 } };

            // Blocking call
            zmq::poll(&items[0], 1, -1);

            if (items[0].revents & ZMQ_POLLIN)
            {
                zmq::multipart_t msg;
                msg.recv(m_client_socket);

                while (!msg.empty()) {
                    const unsigned char* msg_data = msg.pop().data<unsigned char>();
                    std::cout << msg_data << std::endl;
                    // TODO Send message through comms
                }
            }
        }
    }

    /*******************
     * debugger module *
     *******************/

    py::module get_debugger_module_impl()
    {
        py::module debugger_module = py::module("kernel");

        py::class_<xdebugger>(debugger_module, "Debugger")
            .def(py::init<>());

        return debugger_module;
    }

    py::module get_debugger_module()
    {
        static py::module debugger_module = get_debugger_module_impl();
        return debugger_module;
    }

    interpreter& get_interpreter()
    {
        return dynamic_cast<interpreter&>(xeus::get_interpreter());
    }

    void register_debugger_comm()
    {
        auto debugger_start_callback = [] (xeus::xcomm&& comm, const xeus::xmessage& msg) {
            py::object debugger = get_interpreter().start_debugging();

            // On message, forward it to ptvsd and send back the response to the client?
            comm.on_message([] (const xeus::xmessage& msg) {
                std::cout << "I received something!!!" << std::endl;
            });

            // On Comm close, stop the communication?
            // comm.on_close();
        };

        xeus::get_interpreter().comm_manager().register_comm_target(
            "jupyter.debugger", debugger_start_callback
        );
    }
}
