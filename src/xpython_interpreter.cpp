/***************************************************************************
* Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include "Python.h"

#include "pybind11/embed.h"
#include "pybind11/eval.h"
#include "pybind11/stl.h"

#include "xeus/xjson.hpp"

#include "xpyt_config.hpp"
#include "xpython_interpreter.hpp"

namespace py = pybind11;

namespace xpyt
{
    void python_interpreter::configure_impl()
    {
    }

    python_interpreter::python_interpreter(int /*argc*/, const char* const* /*argv*/)
    {
        py::initialize_interpreter();

        py::exec(
            "import sys\n"
            "\n"
            "class XPytLogger(object):\n"
            "    def __init__(self):\n"
            "        self.terminal = sys.stdout\n"
            "        self.log = ''\n"
            "\n"
            "    def write(self, message):\n"
            "        self.terminal.write(message)\n"
            "        self.log += message\n"
            "\n"
            "sys.stdout = XPytLogger()\n"
        );
    }

    python_interpreter::~python_interpreter()
    {
        py::finalize_interpreter();
    }

    xeus::xjson python_interpreter::execute_request_impl(
        int execution_counter,
        const std::string& code,
        bool silent,
        bool store_history,
        const xeus::xjson_node* user_expressions,
        bool allow_stdin)
    {
        xeus::xjson kernel_res;

        try
        {
            py::exec(code);

            std::string stdout = py::eval("sys.stdout.log").cast<std::string>();
            if (!silent && stdout.size() != 0)
            {
                publish_stream("stdout", stdout);
                py::exec("sys.stdout.log = ''");
            }

            kernel_res["status"] = "ok";
            kernel_res["payload"] = xeus::xjson::array();
            kernel_res["user_expressions"] = xeus::xjson::object();
        } catch(const std::exception& e) {

            std::string ename = "Execution error";
            std::string evalue = e.what();
            std::vector<std::string> traceback({ename + ": " + evalue});

            if (!silent)
            {
                publish_execution_error(ename, evalue, traceback);
            }

            kernel_res["status"] = "error";
            kernel_res["ename"] = ename;
            kernel_res["evalue"] = evalue;
            kernel_res["traceback"] = traceback;
        }

        return kernel_res;
    }

    xeus::xjson python_interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        std::cout << code << std::endl;
        std::cout << cursor_pos << std::endl;
        return xeus::xjson::object();
    }

    xeus::xjson python_interpreter::inspect_request_impl(const std::string& /*code*/,
                                                  int /*cursor_pos*/,
                                                  int /*detail_level*/)
    {
        return xeus::xjson::object();
    }

    xeus::xjson python_interpreter::history_request_impl(const xeus::xhistory_arguments& /*args*/)
    {
        return xeus::xjson::object();
    }

    xeus::xjson python_interpreter::is_complete_request_impl(const std::string& /*code*/)
    {
        return xeus::xjson::object();
    }

    xeus::xjson python_interpreter::kernel_info_request_impl()
    {
        xeus::xjson result;
        result["implementation"] = "xeus-python";
        result["implementation_version"] = XPYT_VERSION;

        /* The jupyter-console banner for xeus-cling is the following:
            __   ________ _    _  _____       _______     _________ _    _  ____  _   _
            \ \ / /  ____| |  | |/ ____|     |  __ \ \   / /__   __| |  | |/ __ \| \ | |
             \ V /| |__  | |  | | (___ ______| |__) \ \_/ /   | |  | |__| | |  | |  \| |
              > < |  __| | |  | |\___ \______|  ___/ \   /    | |  |  __  | |  | | . ` |
             / . \| |____| |__| |____) |     | |      | |     | |  | |  | | |__| | |\  |
            /_/ \_\______|\____/|_____/      |_|      |_|     |_|  |_|  |_|\____/|_| \_|

          C++ Jupyter Kernel for Python
        */

        result["banner"] = " __   ________ _    _  _____       _______     _________ _    _  ____  _   _ \n"
                           " \\ \\ / /  ____| |  | |/ ____|     |  __ \\ \\   / /__   __| |  | |/ __ \\| \\ | |\n"
                           "  \\ V /| |__  | |  | | (___ ______| |__) \\ \\_/ /   | |  | |__| | |  | |  \\| |\n"
                           "   > < |  __| | |  | |\\___ \\______|  ___/ \\   /    | |  |  __  | |  | | . ` |\n"
                           "  / . \\| |____| |__| |____) |     | |      | |     | |  | |  | | |__| | |\\  |\n"
                           " /_/ \\_\\______|\\____/|_____/      |_|      |_|     |_|  |_|  |_|\\____/|_| \\_|\n"
                           "\n"
                           "  C++ Jupyter Kernel for Python  ";

        result["language_info"]["name"] = "python";
        result["language_info"]["version"] = "TODO";
        result["language_info"]["mimetype"] = "text/x-python";
        result["language_info"]["file_extension"] = ".py";
        return result;
    }

    void python_interpreter::input_reply_impl(const std::string& /*value*/)
    {
    }
}
