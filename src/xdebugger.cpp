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
#include "xutils.hpp"

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

        void start_ptvsd();
        void start_client();

        void send_to_ptvsd(py::object message);

    private:

        void start_client_impl();

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
        m_client_socket.connect(get_end_point(m_host, m_server_port));
    }

    xdebugger::~xdebugger()
    {
        // TODO: Send stop event to ptvsd
    }

    void xdebugger::start_ptvsd()
    {
        py::module ptvsd = py::module::import("ptvsd");

        ptvsd.attr("enable_attach")(py::make_tuple(m_host, m_server_port), "log_dir"_a="xpython_debug_logs");
    }

    void xdebugger::start_client()
    {
        py::module threading = py::module::import("threading");

        py::object thread = threading.attr("Thread")("target"_a=py::cpp_function([this] () {
            start_client_impl();
        }));
        thread.attr("start")();
    }

    void xdebugger::start_client_impl()
    {
        while (true) // TODO Find a stop condition (ptvsd exit message?)
        {
            // Releasing the GIL before the blocking call
            py::gil_scoped_release release;

            zmq::pollitem_t items[] = { { m_client_socket, 0, ZMQ_POLLIN, 0 } };

            std::cout << green_text("client::waiting for new messages") << std::endl;
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

                    std::cout << green_text("client::sending through comms: ") << static_cast<std::string>(py::str(msg_content)) << std::endl;

                    m_comm.attr("send")("data"_a=msg_content);
                }
            }
        }
    }

    void xdebugger::send_to_ptvsd(py::object message)
    {
        std::string cpp_msg = static_cast<std::string>(py::str(message));
        std::cout << green_text("client::send to ptvsd: ") << cpp_msg << std::endl;
        m_client_socket.send(cpp_msg.cbegin(), cpp_msg.cend());
    }

    interpreter& get_interpreter()
    {
        return dynamic_cast<interpreter&>(xeus::get_interpreter());
    }

    void debugger_callback(py::object comm, py::object msg)
    {
        py::object debugger = get_interpreter().start_debugging(comm);

        // Start client in a secondary thread
        std::cout << red_text("-- start_client") << std::endl;
        debugger.attr("start_client")();

        std::cout << red_text("-- start_ptvsd") << std::endl;
        debugger.attr("start_ptvsd")();

        std::cout << red_text("debugger::successfully initialized") << std::endl;

        // On message, forward it to ptvsd and send back the response to the client?
        comm.attr("on_msg")(py::cpp_function([debugger] (py::object msg) {
            std::cout << blue_text("comm::received: ") << py::str(msg).cast<std::string>() << std::endl;
            debugger.attr("send_to_ptvsd")(msg["content"]["data"]);
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
            .def("start_ptvsd", &xdebugger::start_ptvsd)
            .def("send_to_ptvsd", &xdebugger::send_to_ptvsd);

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
