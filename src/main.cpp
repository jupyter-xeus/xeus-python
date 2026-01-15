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


#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus/xinterpreter.hpp"
#include "xeus/xhelper.hpp"

#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus-zmq/xzmq_context.hpp"



#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xinterpreter_raw.hpp"
#include "xeus-python/xdebugger.hpp"
#include "xeus-python/xpaths.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xutils.hpp"


#include "xeus-python/xasync_runner.hpp"
#include "xeus-zmq/xcontrol_default_runner.hpp"
#include "xeus-zmq/xserver_zmq_split.hpp"

namespace py = pybind11;

int main(int argc, char* argv[])
{
    if (xeus::should_print_version(argc, argv))
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

    // Python initialization
    PyStatus status;

    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    // config.isolated = 1;

    // Setting Program Name
    static const std::string executable(xpyt::get_python_path());
    static const std::wstring wexecutable(executable.cbegin(), executable.cend());
    config.program_name = const_cast<wchar_t*>(wexecutable.c_str());

    // Setting Python Home
    static const std::string pythonhome{ xpyt::get_python_prefix() };
    static const std::wstring wstr(pythonhome.cbegin(), pythonhome.cend());;
    config.home = const_cast<wchar_t*>(wstr.c_str());
    xpyt::print_pythonhome();

    // Instantiating the Python interpreter
    py::scoped_interpreter guard{};
    py::gil_scoped_acquire acquire;



    // Setting argv
    wchar_t** argw = new wchar_t*[size_t(argc)];
    for(auto i = 0; i < argc; ++i)
    {
        argw[i] = Py_DecodeLocale(argv[i], nullptr);
    }
    PyWideStringList py_argw;
    py_argw.length = argc;
    py_argw.items = argw;

    config.argv = py_argw;
    config.parse_argv = 1;

    for(auto i = 0; i < argc; ++i)
    {
        PyMem_RawFree(argw[i]);
    }
    delete[] argw;



    std::unique_ptr<xeus::xcontext> context = xeus::make_zmq_context();

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

    std::string connection_filename = xeus::extract_filename(argc, argv);

#ifdef XEUS_PYTHON_PYPI_WARNING
    std::clog <<
        "WARNING: this instance of xeus-python has been installed from a PyPI wheel.\n"
        "We recommend using a general-purpose package manager instead, such as Conda/Mamba.\n"
        << std::endl;
#endif

    nl::json debugger_config;
    debugger_config["python"] = executable;







    
    // auto make_xserver = [&](xeus::xcontext& context,
    //                                const xeus::xconfiguration& config,
    //                                nl::json::error_handler_t eh) {
    //     return xeus::make_xserver_uv(context, config, eh, loop_ptr, std::move(py_hook));
    // };




    // create the runner by hand

    

    auto make_xserver = [](
        xeus::xcontext& context,
        const xeus::xconfiguration& config,
         nl::json::error_handler_t eh)
    {
        std::cout << "Creating xserver_uv" << std::endl;
        return xeus::make_xserver_shell
        (
            context,
            config,
            eh,
            std::make_unique<xeus::xcontrol_default_runner>(),
            std::make_unique<xpyt::xasync_runner>()
        );
    };










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
                                                       xeus::make_file_logger(xeus::xlogger::content, "xeus.log")),
                             xpyt::make_python_debugger,
                             debugger_config);

        std::clog <<
            "Starting xeus-python kernel...\n\n"
            "If you want to connect to this kernel from an other client, you can use"
            " the " + connection_filename + " file."
            << std::endl;
            
        std::cout << "Starting kernel..." << std::endl;
        kernel.start();
        std::cout << "Kernel stopped." << std::endl;
    }
    else
    {
        std::clog << "Instantiating kernel" << std::endl;
        xeus::xkernel kernel(xeus::get_user_name(),
                             std::move(context),
                             std::move(interpreter),
                             make_xserver,
                             std::move(hist),
                             nullptr,
                             xpyt::make_python_debugger,
                             debugger_config);

        std::cout << "Getting config" << std::endl;
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

        std::cout << "Starting kernel..." << std::endl;
        kernel.start();
        std::cout << "Kernel stopped." << std::endl;
    }

    return 0;
}
