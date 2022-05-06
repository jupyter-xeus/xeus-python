/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11/functional.h"

#include "pybind11_json/pybind11_json.hpp"

#include "xeus-python/xinterpreter_raw.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xtraceback.hpp"
#include "xeus-python/xutils.hpp"

#include "xcomm.hpp"
#include "xkernel.hpp"
#include "xdisplay.hpp"
#include "xinput.hpp"
#include "xinternal_utils.hpp"
#include "xstream.hpp"
#include "xinspect.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    raw_interpreter::raw_interpreter(bool redirect_output_enabled /*=true*/, bool redirect_display_enabled /*=true*/) :m_redirect_display_enabled{ redirect_display_enabled }

    {
        xeus::register_interpreter(this);
        if (redirect_output_enabled)
        {
            redirect_output();
        }
    }

    raw_interpreter::~raw_interpreter()
    {
    }

    void raw_interpreter::configure_impl()
    {
        if (m_release_gil_at_startup)
        {
            // The GIL is not held by default by the interpreter, so every time we need to execute Python code we
            // will need to acquire the GIL
            m_release_gil = gil_scoped_release_ptr(new py::gil_scoped_release());
        }

        py::gil_scoped_acquire acquire;

        py::module sys = py::module::import("sys");
        py::module jedi = py::module::import("jedi");
        jedi.attr("api").attr("environment").attr("get_default_environment") = py::cpp_function([jedi]() {
            jedi.attr("api").attr("environment").attr("SameEnvironment")();
        });

        py::module display_module = get_display_module(true);
        m_displayhook = display_module.attr("DisplayHook")();

        if (m_redirect_display_enabled)
        {
            sys.attr("displayhook") = m_displayhook;
        }

        // Expose display functions to Python
        py::globals()["display"] = display_module.attr("display");
        py::globals()["update_display"] = display_module.attr("update_display");
        // Monkey patching "import IPython.core.display"
        sys.attr("modules")["IPython.core.display"] = display_module;

        py::module kernel_module = get_kernel_module(true);
        // Monkey patching "from ipykernel.comm import Comm"
        sys.attr("modules")["ipykernel.comm"] = kernel_module;

        // Monkey patching "from IPython import get_ipython"
        sys.attr("modules")["IPython.core.getipython"] = kernel_module;

        // Add get_ipython to global namespace
        py::globals()["get_ipython"] = kernel_module.attr("get_ipython");
        kernel_module.attr("get_ipython")();

        py::globals()["_i"] = "";
        py::globals()["_ii"] = "";
        py::globals()["_iii"] = "";
    }

    nl::json raw_interpreter::execute_request_impl(
        int execution_count,
        const std::string& code,
        bool silent,
        bool /*store_history*/,
        nl::json /*user_expressions*/,
        bool allow_stdin)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
        py::str code_copy;
        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(allow_stdin);
        code_copy = code;
        try
        {
            // Import modules
            py::module ast = py::module::import("ast");
            py::module builtins = py::module::import("builtins");

            // Parse code to AST
            py::object code_ast = ast.attr("parse")(code_copy, "<string>", "exec");
            py::list expressions = code_ast.attr("body");

            std::string filename = get_cell_tmp_file(code);
            register_filename_mapping(filename, execution_count);


            // If the last statement is an expression, we compile it separately
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

                if (m_displayhook.ptr() != nullptr)
                {
                    m_displayhook.attr("set_execution_count")(execution_count);
                }

                exec(compiled_code);
                exec(compiled_interactive_code);
            }
            else
            {
                py::object compiled_code = builtins.attr("compile")(code_ast, filename, "exec");
                exec(compiled_code);
            }

            kernel_res["status"] = "ok";
            kernel_res["user_expressions"] = nl::json::object();
            kernel_res["payload"] = nl::json::array();

        }
        catch (py::error_already_set& e)
        {
            xerror error = extract_already_set_error(e);

            if (error.m_ename == "SyntaxError")
            {
                if (code.find("%") != std::string::npos)
                {
                    error.m_traceback.push_back("There may be Ipython magics in your code, this feature is not supported in xeus-python raw mode! Please consider switching to xeus-python normal mode or removing these magics");
                }
            }

            if (!silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            kernel_res["status"] = "error";
            kernel_res["ename"] = error.m_ename;
            kernel_res["evalue"] = error.m_evalue;
            kernel_res["traceback"] = error.m_traceback;
        }

        // Cache inputs
        py::globals()["_iii"] = py::globals()["_ii"];
        py::globals()["_ii"] = py::globals()["_i"];
        py::globals()["_i"] = code;

        return kernel_res;
    }

    nl::json raw_interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
        std::vector<std::string> matches;
        int cursor_start = cursor_pos;

        py::list completions = get_completions(code, cursor_pos);

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
        kernel_res["metadata"] = nl::json::object();
        kernel_res["status"] = "ok";
        return kernel_res;
    }

    nl::json raw_interpreter::inspect_request_impl(const std::string& code,
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

    nl::json raw_interpreter::is_complete_request_impl(const std::string&)
    {
        nl::json result;
        result["status"] = "complete";
        return result;
    }

    nl::json raw_interpreter::kernel_info_request_impl()
    {
        nl::json result;
        result["implementation"] = "xeus-python";
        result["implementation_version"] = XPYT_VERSION;

        /* The jupyter-console banner for xeus-python is the following:
          __  _____ _   _ ___
          \ \/ / _ \ | | / __|
           >  <  __/ |_| \__ \
          /_/\_\___|\__,_|___/

          xeus-python: a Jupyter lernel for Python
        */

        std::string banner = ""
            "  __  _____ _   _ ___\n"
            "  \\ \\/ / _ \\ | | / __|\n"
            "   >  <  __/ |_| \\__ \\\n"
            "  /_/\\_\\___|\\__,_|___/\n"
            "\n"
            "  xeus-python: a Jupyter kernel for Python\n"
            "  Python ";
        banner.append(PY_VERSION);
#ifdef XEUS_PYTHON_PYPI_WARNING
        banner.append("\n"
            "\n"
            "WARNING: this instance of xeus-python has been installed from a PyPI wheel.\n"
            "We recommend using a general-purpose package manager instead, such as Conda/Mamba."
            "\n");
#endif
        result["banner"] = banner;
        result["debugger"] = true;

        result["language_info"]["name"] = "python";
        result["language_info"]["version"] = PY_VERSION;
        result["language_info"]["mimetype"] = "text/x-python";
        result["language_info"]["file_extension"] = ".py";

        result["help_links"] = nl::json::array();
        result["help_links"][0] = nl::json::object(
            {
                {"text", "Xeus-Python Reference"},
                {"url", "https://xeus-python.readthedocs.io"}
            }
        );

        result["status"] = "ok";
        return result;
    }

    void raw_interpreter::shutdown_request_impl()
    {
    }


    void raw_interpreter::redirect_output()
    {
        py::module sys = py::module::import("sys");
        py::module stream_module = get_stream_module();

        sys.attr("stdout") = stream_module.attr("Stream")("stdout");
        sys.attr("stderr") = stream_module.attr("Stream")("stderr");
    }

}
