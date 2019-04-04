/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11/functional.h"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xeus_python_config.hpp"

#include "xcomm.hpp"
#include "xdisplay.hpp"
#include "xdebugger.hpp"
#include "xinput.hpp"
#include "xinspect.hpp"
#include "xis_complete.hpp"
#include "xstream.hpp"
#include "xtraceback.hpp"
#include "xutils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{
    void interpreter::configure_impl()
    {
        py::gil_scoped_acquire acquire;

        // Initialize Jedi for autocompletion
        py::module jedi = py::module::import("jedi");
        jedi.attr("api").attr("environment").attr("get_default_environment") = py::cpp_function([jedi] () {
            jedi.attr("api").attr("environment").attr("SameEnvironment")();
        });

        // Register debugger Comm target
        register_debugger_comm();
    }

    interpreter::interpreter(int /*argc*/, const char* const* /*argv*/)
    {
        py::gil_scoped_acquire acquire;
        xeus::register_interpreter(this);

        redirect_output();
        redirect_display();

        py::module sys = py::module::import("sys");
        py::module kernel_module = get_kernel_module();

        // Monkey patching "from ipykernel.comm import Comm"
        sys.attr("modules")["ipykernel.comm"] = kernel_module;

        // Monkey patching "from IPython.display import display"
        sys.attr("modules")["IPython.display"] = get_display_module();

        // Monkey patching "from IPython import get_ipython"
        sys.attr("modules")["IPython.core.getipython"] = kernel_module;
    }

    interpreter::~interpreter()
    {
    }

    nl::json interpreter::execute_request_impl(int execution_count,
                                               const std::string& code,
                                               bool silent,
                                               bool /*store_history*/,
                                               nl::json /*user_expressions*/,
                                               bool allow_stdin)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
        m_inputs.push_back(code);

        if (code.size() >= 2 && code[0] == '?')
        {
            std::string result = formatted_docstring(code);
            if (result.empty())
            {
                result = "Object " + code.substr(1) + " not found.";
            }

            kernel_res["status"] = "ok";
            kernel_res["payload"] = nl::json::array();
            kernel_res["payload"][0] = nl::json::object({
                {"data", {
                    {"text/plain", result}
                }},
                {"source", "page"},
                {"start", 0}
            });
            kernel_res["user_expressions"] = nl::json::object();

            return kernel_res;
        }

        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(allow_stdin);

        try
        {
            // Import modules
            py::module ast = py::module::import("ast");
            py::module builtins = py::module::import(XPYT_BUILTINS);

            // Parse code to AST
            py::object code_ast = ast.attr("parse")(code, "<string>", "exec");
            py::list expressions = code_ast.attr("body");

            std::string filename = "[" + std::to_string(execution_count) + "]";

            // If the last statement is an expression, we compile it seperately
            // in an interactive mode (This will trigger the display hook)
            py::object last_stmt = expressions[py::len(expressions) - 1];
            if (py::isinstance(last_stmt, ast.attr("Expr")))
            {
                code_ast.attr("body").attr("pop")();

                py::list interactive_nodes;
                interactive_nodes.append(last_stmt);

                py::object interactive_ast = ast.attr("Interactive")(interactive_nodes);

                py::object compiled_code = builtins.attr("compile")(code_ast, filename, "exec");
                py::object compiled_interactive_code = builtins.attr("compile")(interactive_ast, filename, "single");

                m_displayhook.attr("set_execution_count")(execution_count);

                exec(compiled_code);
                exec(compiled_interactive_code);
            }
            else
            {
                py::object compiled_code = builtins.attr("compile")(code_ast, filename, "exec");
                exec(compiled_code);
            }

            kernel_res["status"] = "ok";
            kernel_res["payload"] = nl::json::array();
            kernel_res["user_expressions"] = nl::json::object();
        }
        catch (py::error_already_set& e)
        {
            xerror error = extract_error(e, m_inputs);

            if (!silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            kernel_res["status"] = "error";
            kernel_res["ename"] = error.m_ename;
            kernel_res["evalue"] = error.m_evalue;
            kernel_res["traceback"] = error.m_traceback;
        }

        return kernel_res;
    }

    nl::json interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
        std::vector<std::string> matches;
        int cursor_start = cursor_pos;

        py::list completions = static_inspect(code, cursor_pos).attr("completions")();

        if (py::len(completions) != 0)
        {
            cursor_start -= py::len(completions[0].attr("name_with_symbols")) - py::len(completions[0].attr("complete"));
            for (py::handle completion : completions)
            {
                matches.push_back(completion.attr("name_with_symbols").cast<std::string>());
            }
        }

        kernel_res["cursor_start"] = cursor_start;
        kernel_res["cursor_end"] = cursor_pos;
        kernel_res["matches"] = matches;
        kernel_res["status"] = "ok";
        return kernel_res;
    }

    nl::json interpreter::inspect_request_impl(const std::string& code,
                                               int cursor_pos,
                                               int /*detail_level*/)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
        nl::json pub_data;

        std::string docstring = formatted_docstring(code, cursor_pos);

        bool found = false;
        if (!docstring.empty())
        {
            found = true;
            pub_data["text/plain"] = docstring;
        }

        kernel_res["data"] = pub_data;
        kernel_res["metadata"] = nl::json::object();
        kernel_res["found"] = found;
        kernel_res["status"] = "ok";
        return kernel_res;
    }

    nl::json interpreter::is_complete_request_impl(const std::string& code)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;

        py::module completion_module = get_completion_module();
        py::list result = completion_module.attr("check_complete")(code);

        auto status = result[0].cast<std::string>();

        kernel_res["status"] = status;
        if (status.compare("incomplete") == 0)
        {
            kernel_res["indent"] = std::string(result[1].cast<std::size_t>(), ' ');
        }
        return kernel_res;
    }

    nl::json interpreter::kernel_info_request_impl()
    {
        nl::json result;
        result["implementation"] = "xeus-python";
        result["implementation_version"] = XPYT_VERSION;

        /* The jupyter-console banner for xeus-python is the following:
            __   ________ _    _  _____       _______     _________ _    _  ____  _   _
            \ \ / /  ____| |  | |/ ____|     |  __ \ \   / /__   __| |  | |/ __ \| \ | |
             \ V /| |__  | |  | | (___ ______| |__) \ \_/ /   | |  | |__| | |  | |  \| |
              > < |  __| | |  | |\___ \______|  ___/ \   /    | |  |  __  | |  | | . ` |
             / . \| |____| |__| |____) |     | |      | |     | |  | |  | | |__| | |\  |
            /_/ \_\______|\____/|_____/      |_|      |_|     |_|  |_|  |_|\____/|_| \_|

          C++ Jupyter Kernel for Python
        */

        std::string banner = ""
              " __   ________ _    _  _____       _______     _________ _    _  ____  _   _ \n"
              " \\ \\ / /  ____| |  | |/ ____|     |  __ \\ \\   / /__   __| |  | |/ __ \\| \\ | |\n"
              "  \\ V /| |__  | |  | | (___ ______| |__) \\ \\_/ /   | |  | |__| | |  | |  \\| |\n"
              "   > < |  __| | |  | |\\___ \\______|  ___/ \\   /    | |  |  __  | |  | | . ` |\n"
              "  / . \\| |____| |__| |____) |     | |      | |     | |  | |  | | |__| | |\\  |\n"
              " /_/ \\_\\______|\\____/|_____/      |_|      |_|     |_|  |_|  |_|\\____/|_| \\_|\n"
              "\n"
              "  C++ Jupyter Kernel for Python  \n"
              "  Python ";
        banner.append(PY_VERSION);
        result["banner"] = banner;

        result["language_info"]["name"] = "python";
        result["language_info"]["version"] = PY_VERSION;
        result["language_info"]["mimetype"] = "text/x-python";
        result["language_info"]["file_extension"] = ".py";
        return result;
    }

    void interpreter::shutdown_request_impl()
    {
    }

    void interpreter::redirect_output()
    {
        py::module sys = py::module::import("sys");
        py::module stream_module = get_stream_module();

        sys.attr("stdout") = stream_module.attr("Stream")("stdout");
        sys.attr("stderr") = stream_module.attr("Stream")("stderr");
    }

    void interpreter::redirect_display()
    {
        py::module sys = py::module::import("sys");
        py::module display_module = get_display_module();

        m_displayhook = display_module.attr("DisplayHook")();

        sys.attr("displayhook") = m_displayhook;
        py::globals()["display"] = display_module.attr("display");
    }
}
