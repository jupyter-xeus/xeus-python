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
#include "xcomm.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

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

        xdebugger(py::object comm);
        virtual ~xdebugger();

        void start_server();
        void start_client();

    private:

        zmq::context_t m_context;
        std::string m_host;
        std::size_t m_server_port;
        zmq::socket_t m_client_socket;
        py::object m_comm;
        py::object m_secondary_thread;
    };

    /****************************
     * xdebugger implementation *
     ****************************/

    xdebugger::xdebugger(py::object comm)
        : m_host("127.0.0.1")
        , m_server_port(5678) // Hardcoded for now, we need a way to find an available port
        , m_client_socket(m_context, zmq::socket_type::stream)
        , m_comm(comm)
    {
    }

    xdebugger::~xdebugger()
    {
        // TODO: Send stop event to ptvsd
    }

    void xdebugger::start_server()
    {
        py::module ptvsd = py::module::import("ptvsd");

        ptvsd.attr("enable_attach")(py::make_tuple(m_host, m_server_port), "log_dir"_a="xpython_debug_logs");
    }

    void xdebugger::start_client()
    {
        m_client_socket.connect(get_end_point(m_host, m_server_port));

        while (true) // TODO Find a stop condition (ptvsd exit message?)
        {
            // Releasing the GIL before the blocking call
            py::gil_scoped_release release;

            zmq::pollitem_t items[] = { { m_client_socket, 0, ZMQ_POLLIN, 0 } };
            // Blocking call
            zmq::poll(&items[0], 1, -1);

            if (items[0].revents & ZMQ_POLLIN)
            {
                zmq::multipart_t multipart;
                multipart.recv(m_client_socket);

                while (!multipart.empty()) {
                    // Acquire the GIL before executing Python code
                    py::gil_scoped_acquire acquire;

                    zmq::message_t msg = multipart.pop();
                    py::object msg_content;

                    try
                    {
                        py::module json = py::module::import("json");
                        const char* buf = msg.data<const char>();
                        std::string msg_string(buf, msg.size());

                        msg_content = json.attr("loads")(msg_string);
                    }
                    catch (...)
                    {
                        // Could not decode the message, not sending anything
                        continue;
                    }

                    std::cout << "client::Sending through comms: "<< static_cast<std::string>(py::str(msg_content)) << std::endl;

                    m_comm.attr("send")("data"_a=msg_content);
                }
            }
        }
    }

    interpreter& get_interpreter()
    {
        return dynamic_cast<interpreter&>(xeus::get_interpreter());
    }

    void debugger_callback(py::object comm, py::object msg)
    {
        py::object debugger = get_interpreter().start_debugging(comm);

        // Start client in a secondary thread
        std::cout << "-- start_client" << std::endl;
        py::module threading = py::module::import("threading");
        py::object thread = threading.attr("Thread")("target"_a=debugger.attr("start_client"));
        thread.attr("start")();

        std::cout << "-- start_server" << std::endl;
        debugger.attr("start_server")();

        std::cout << "Successfully initialized the debugger" << std::endl;

        // On message, forward it to ptvsd and send back the response to the client?
        comm.attr("on_msg")(py::cpp_function([debugger] (py::object msg) {
            std::cout << "comm::received: " << py::str(msg).cast<std::string>() << std::endl;
            // TODO send msg.content.data with debugger.socket
        }));

        // On Comm close, stop the communication?
        // comm.on_close();
    }

    /*******************
     * debugger module *
     *******************/

    py::module get_debugger_module_impl()
    {
        py::module debugger_module = py::module("debugger");

        py::class_<xdebugger>(debugger_module, "Debugger")
            .def(py::init<py::object>())
            .def("start_client", &xdebugger::start_client)
            .def("start_server", &xdebugger::start_server);

        debugger_module.def("debugger_callback", &debugger_callback);

        return debugger_module;
    }

    py::module get_debugger_module()
    {
        static py::module debugger_module = get_debugger_module_impl();
        return debugger_module;
    }

    void register_debugger_comm()
    {
        get_kernel_module().attr("register_target")(
            "jupyter.debugger", get_debugger_module().attr("debugger_callback")
        );
    }
}
