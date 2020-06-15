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
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11/functional.h"

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xeus_python_config.hpp"

#include "xcomm.hpp"
#include "xdisplay.hpp"
#include "xinput.hpp"
#include "xinspect.hpp"
#include "xis_complete.hpp"
#include "xlinecache.hpp"
#include "xstream.hpp"
#include "xtraceback.hpp"
#include "xutils.hpp"
#include "xinteractiveshell.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{
    void interpreter::configure_impl()
    {
        // The GIL is not held by default by the interpreter, so every time we need to execute Python code we
        // will need to acquire the GIL
        m_release_gil = gil_scoped_release_ptr(new py::gil_scoped_release());

        py::gil_scoped_acquire acquire;
        py::module jedi = py::module::import("jedi");
        jedi.attr("api").attr("environment").attr("get_default_environment") = py::cpp_function([jedi] () {
            jedi.attr("api").attr("environment").attr("SameEnvironment")();
        });
    }

    interpreter::interpreter(bool redirect_output_enabled/*=true*/, bool redirect_display_enabled/*=true*/)
    {
        xeus::register_interpreter(this);
        if (redirect_output_enabled)
        {
            redirect_output();
        }
        redirect_display(redirect_display_enabled);

        py::module sys = py::module::import("sys");

        // Monkey patching "import linecache". This monkey patch does not work with Python2.
#if PY_MAJOR_VERSION >= 3
        sys.attr("modules")["linecache"] = get_linecache_module();
#endif

        // Monkey patching "from ipykernel.comm import Comm"
        sys.attr("modules")["ipykernel.comm"] = get_kernel_module();

        // Monkey patching "import IPython.core.display"
        sys.attr("modules")["IPython.core.display"] = get_display_module();

        // Monkey patching "from IPython import get_ipython"
        sys.attr("modules")["IPython.core.getipython"] = get_kernel_module();


        // add get_ipython to global namespace
        exec(py::str("from IPython.core.getipython import get_ipython"));
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

        py::module input_transformers = py::module::import("IPython.core.inputtransformer2");
        py::object transformer_manager = input_transformers.attr("TransformerManager")();
        py::str code_copy = transformer_manager.attr("transform_cell")(code);

        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(allow_stdin);

        try
        {
            // Import modules
            py::module ast = py::module::import("ast");
            py::module builtins = py::module::import(XPYT_BUILTINS);

            // Parse code to AST
            py::object code_ast = ast.attr("parse")(code_copy, "<string>", "exec");
            py::list expressions = code_ast.attr("body");

            std::string filename = get_cell_tmp_file(code);
            register_filename_mapping(filename, execution_count);

            // Caching the input code
#if PY_MAJOR_VERSION >= 3
            py::module linecache = py::module::import("linecache");
            linecache.attr("xupdatecache")(code, filename);
#endif

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

            xinteractive_shell * xshell = get_kernel_module()
                .attr("get_ipython")()
                .cast<xinteractive_shell *>();
            auto payload = xshell->get_payloads();

            kernel_res["status"] = "ok";
            kernel_res["payload"] = payload;
            kernel_res["user_expressions"] = nl::json::object();

            xshell->clear_payloads();
        }
        catch (py::error_already_set& e)
        {
            xerror error = extract_error(e);

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
        result["help_links"][0] = nl::json::object({
            {"text", "Xeus-Python Reference"},
            {"url", "https://xeus-python.readthedocs.io"}
        });

        result["status"] = "ok";
        return result;
    }

    void interpreter::shutdown_request_impl()
    {
    }

    nl::json interpreter::internal_request_impl(const nl::json& content)
    {
        py::gil_scoped_acquire acquire;
        std::string code = content.value("code", "");
        nl::json reply;
        try
        {
            // Import modules
            py::module ast = py::module::import("ast");
            py::module builtins = py::module::import(XPYT_BUILTINS);

            // Parse code to AST
            py::object code_ast = ast.attr("parse")(code, "<string>", "exec");

            std::string filename = "debug_this_thread";
            py::object compiled_code = builtins.attr("compile")(code_ast, filename, "exec");
            exec(compiled_code);

            reply["status"] = "ok";
        }
        catch (py::error_already_set& e)
        {
            xerror error = extract_error(e);

            publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            error.m_traceback.resize(1);
            error.m_traceback[0] = code;

            reply["status"] = "error";
            reply["ename"] = error.m_ename;
            reply["evalue"] = error.m_evalue;
            reply["traceback"] = error.m_traceback;
        }
        return reply;
    }

    void interpreter::redirect_output()
    {
        py::module sys = py::module::import("sys");
        py::module stream_module = get_stream_module();

        sys.attr("stdout") = stream_module.attr("Stream")("stdout");
        sys.attr("stderr") = stream_module.attr("Stream")("stderr");
    }

    void interpreter::redirect_display(bool install_hook/*=true*/)
    {
        py::module display_module = get_display_module();
        m_displayhook = display_module.attr("DisplayHook")();
        if (install_hook)
        {
            py::module sys = py::module::import("sys");
            sys.attr("displayhook") = m_displayhook;
        }
        py::globals()["display"] = display_module.attr("display");
    }
}
