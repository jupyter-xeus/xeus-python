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
#include <iostream>
#include <string>
#include <utility>
#include <signal.h>

#ifdef __GNUC__
#include <stdio.h>
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#ifndef UVW_AS_LIB
#define UVW_AS_LIB
#include <uvw.hpp>
#endif

#include "xeus/xeus_context.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus/xinterpreter.hpp"

#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "xeus-zmq/hook_base.hpp"

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xinterpreter_raw.hpp"
#include "xeus-python/xdebugger.hpp"
#include "xeus-python/xpaths.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xutils.hpp"

namespace py = pybind11;

class py_hook : public xeus::hook_base
{
public:
    py_hook() = default;

    void pre_hook() override
    {
        // p_release = new py::gil_scoped_release();
        // p_acquire = new py::gil_scoped_acquire();
        std::cout << "Prehook\n";
    }

    void post_hook() override
    {
        // delete p_acquire;
        // delete p_release;
        // p_acquire = nullptr;
        // p_release = nullptr;
        std::cout << "Posthook\n";

    }

    void run(std::shared_ptr<uvw::loop>) override
    {
        std::cout << "Overriden run\n";

        py::gil_scoped_acquire acquire;
        std::cout << "After acquire gil\n";
        py::exec(R"(
            import asyncio
            loop = asyncio.get_event_loop()
            print('got a loop', loop)
            loop.run_forever()
        )");
    }

private:
    py::gil_scoped_acquire* p_acquire{ nullptr };
    py::gil_scoped_release* p_release{ nullptr };
};



int main(int argc, char* argv[])
{
    if (xpyt::should_print_version(argc, argv))
    {
        std::clog << "xpython " << XPYT_VERSION << std::endl;
        return 0;
    }

    // If we are called from the Jupyter launcher, silence all logging. This
    // is important for a JupyterHub configured with cleanup_servers = False:
    // Upon restart, spawned single-user servers keep running but without the
    // std* streams. When a user then tries to start a new kernel, xpython
    // will get a SIGPIPE and exit.
    if (std::getenv("JPY_PARENT_PID") != NULL)
    {
        std::clog.setstate(std::ios_base::failbit);
    }

    // Registering SIGSEGV handler
#ifdef __GNUC__
    std::clog << "registering handler for SIGSEGV" << std::endl;
    signal(SIGSEGV, xpyt::sigsegv_handler);

    // Registering SIGINT and SIGKILL handlers
    signal(SIGKILL, xpyt::sigkill_handler);
#endif
    signal(SIGINT, xpyt::sigkill_handler);

    // Setting Program Name
    static const std::string executable(xpyt::get_python_path());
    static const std::wstring wexecutable(executable.cbegin(), executable.cend());

    // On windows, sys.executable is not properly set with Py_SetProgramName
    // Cf. https://bugs.python.org/issue34725
    // A private undocumented API was added as a workaround in Python 3.7.2.
    // _Py_SetProgramFullPath(const_cast<wchar_t*>(wexecutable.c_str()));
    Py_SetProgramName(const_cast<wchar_t*>(wexecutable.c_str()));

    // Setting PYTHONHOME
    xpyt::set_pythonhome();
    xpyt::print_pythonhome();

    // Instantiating the Python interpreter
    py::scoped_interpreter guard{};

    uv_loop_t* uv_loop_ptr{ nullptr };

    {
        py::gil_scoped_acquire acquire;

        // Instantiating the loop manually
        py::exec(R"(
            import asyncio
            import uvloop
            from uvloop.loop import libuv_get_loop_t_ptr

            print('Creating uvloop event loop')
            loop = uvloop.new_event_loop()
            asyncio.set_event_loop(loop)
            print('uvloop event loop created')
        )");

        py::object py_loop_ptr = py::eval("libuv_get_loop_t_ptr(loop)");
        void* raw_ptr = PyCapsule_GetPointer(py_loop_ptr.ptr(), nullptr);
        if (!raw_ptr)
        {
            std::cout << "Got null pointer!\n";
            throw std::runtime_error("Failed to get libuv loop pointer");
        }

        uv_loop_ptr = static_cast<uv_loop_t*>(raw_ptr);
        std::cout << "Got a pointer!" << uv_loop_ptr << '\n';
    }

    if (!uv_loop_ptr)
    {
        throw std::runtime_error("Failed to get libuv loop pointer");
        std::cout << "This pointer is no good\n";
    }
    auto loop_ptr = uvw::loop::create(uv_loop_ptr);

    std::cout << "Got a loop!\n";

    // Setting argv
    wchar_t** argw = new wchar_t*[size_t(argc)];
    for(auto i = 0; i < argc; ++i)
    {
        argw[i] = Py_DecodeLocale(argv[i], nullptr);
    }
    PySys_SetArgvEx(argc, argw, 0);
    for(auto i = 0; i < argc; ++i)
    {
        PyMem_RawFree(argw[i]);
    }
    delete[] argw;

    auto context = xeus::make_zmq_context();

    // Instantiating the xeus xinterpreter
    bool raw_mode = xpyt::extract_option("-r", "--raw", argc, argv);
    using interpreter_ptr = std::unique_ptr<xeus::xinterpreter>;
    interpreter_ptr interpreter;
    if (raw_mode)
    {
        interpreter = interpreter_ptr(new xpyt::raw_interpreter());
    }
    else
    {
        interpreter = interpreter_ptr(new xpyt::interpreter());
    }

    using history_manager_ptr = std::unique_ptr<xeus::xhistory_manager>;
    history_manager_ptr hist = xeus::make_in_memory_history_manager();

    std::string connection_filename = xpyt::extract_parameter("-f", argc, argv);

#ifdef XEUS_PYTHON_PYPI_WARNING
    std::clog <<
        "WARNING: this instance of xeus-python has been installed from a PyPI wheel.\n"
        "We recommend using a general-purpose package manager instead, such as Conda/Mamba.\n"
        << std::endl;
#endif

    nl::json debugger_config;
    debugger_config["python"] = executable;

    auto hook = std::make_unique<py_hook>();

    std::cout << "Making a server\n";

    auto make_xserver = [&](xeus::xcontext& context,
                                   const xeus::xconfiguration& config,
                                   nl::json::error_handler_t eh) {
        return xeus::make_xserver_uv_shell_main(context, config, eh, loop_ptr, std::move(hook));
    };

    std::cout << "Done with the lambda\n";

    if (!connection_filename.empty())
    {
        xeus::xconfiguration config = xeus::load_configuration(connection_filename);

        xeus::xkernel kernel(config,
                             xeus::get_user_name(),
                             std::move(context),
                             std::move(interpreter),
                             make_xserver,
                             std::move(hist),
                             xeus::make_console_logger(xeus::xlogger::msg_type,
                                                       xeus::make_file_logger(xeus::xlogger::content, "xeus.log")));
                            //  xpyt::make_python_debugger,
                            //  debugger_config);

        std::clog <<
            "Starting xeus-python kernel...\n\n"
            "If you want to connect to this kernel from an other client, you can use"
            " the " + connection_filename + " file."
            << std::endl;

        std::cout << "Starting kernel\n";

        kernel.start();

        std::cout << "Kernel started\n";

    }
    else
    {
        xeus::xkernel kernel(xeus::get_user_name(),
                             std::move(context),
                             std::move(interpreter),
                             make_xserver,
                             std::move(hist),
                             nullptr,
                             xpyt::make_python_debugger,
                             debugger_config);

        const auto& config = kernel.get_config();
        std::clog <<
            "Starting xeus-python kernel...\n\n"
            "If you want to connect to this kernel from an other client, just copy"
            " and paste the following content inside of a `kernel.json` file. And then run for example:\n\n"
            "# jupyter console --existing kernel.json\n\n"
            "kernel.json\n```\n{\n"
            "    \"transport\": \"" + config.m_transport + "\",\n"
            "    \"ip\": \"" + config.m_ip + "\",\n"
            "    \"control_port\": " + config.m_control_port + ",\n"
            "    \"shell_port\": " + config.m_shell_port + ",\n"
            "    \"stdin_port\": " + config.m_stdin_port + ",\n"
            "    \"iopub_port\": " + config.m_iopub_port + ",\n"
            "    \"hb_port\": " + config.m_hb_port + ",\n"
            "    \"signature_scheme\": \"" + config.m_signature_scheme + "\",\n"
            "    \"key\": \"" + config.m_key + "\"\n"
            "}\n```"
            << std::endl;

        kernel.start();
    }

    return 0;
}
