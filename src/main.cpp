/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus/xserver.hpp"

#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xpythonhome.hpp"
#include "xdebugger.hpp"

namespace py = pybind11;

std::string extract_filename(int& argc, char* argv[])
{
    std::string res = "";
    for (int i = 0; i < argc; ++i)
    {
        if ((std::string(argv[i]) == "-f") && (i + 1 < argc))
        {
            res = argv[i + 1];
            for (int j = i; j < argc - 2; ++j)
            {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            break;
        }
    }
    return res;
}

void print_python_home()
{
#if PY_MAJOR_VERSION == 2
    char* mbstr = Py_GetPythonHome();
#else
    std::setlocale(LC_ALL, "en_US.utf8");
    wchar_t* ph = Py_GetPythonHome();
    
    char mbstr[1024];
    std::wcstombs(mbstr, ph, 1024);
#endif
    std::clog << "PYTHONHOME set to " << mbstr << std::endl;
}

int main(int argc, char* argv[])
{
    std::string file_name = extract_filename(argc, argv);
    xpyt::set_pythonhome();
    print_python_home();

    py::scoped_interpreter guard;
    using interpreter_ptr = std::unique_ptr<xpyt::interpreter>;
    interpreter_ptr interpreter = interpreter_ptr(new xpyt::interpreter(argc, argv));

    using history_manager_ptr = std::unique_ptr<xeus::xhistory_manager>;
    history_manager_ptr hist = xeus::make_in_memory_history_manager();

    if (!file_name.empty())
    {
        xeus::xconfiguration config = xeus::load_configuration(file_name);

        xeus::xkernel kernel(config,
                             xeus::get_user_name(),
                             std::move(interpreter),
                             std::move(hist),
                             xeus::make_console_logger(xeus::xlogger::msg_type,
                                                       xeus::make_file_logger(xeus::xlogger::content, "xeus.log")),
                             xeus::make_xserver_shell_main,
                             xpyt::make_python_debugger);

        std::clog <<
            "Starting xeus-python kernel...\n\n"
            "If you want to connect to this kernel from an other client, you can use"
            " the " + file_name + " file."
            << std::endl;

        kernel.start();
    }
    else
    {
        xeus::xkernel kernel(xeus::get_user_name(),
                             std::move(interpreter),
                             std::move(hist),
                             nullptr,
                             xeus::make_xserver_shell_main,
                             xpyt::make_python_debugger);

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
            "}\n```\n";

        kernel.start();
    }

    return 0;
}
