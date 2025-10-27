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
#include "xeus/xhelper.hpp"

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

    raw_interpreter::raw_interpreter(bool redirect_output_enabled /*=true*/, bool redirect_display_enabled /*=true*/)
        :m_redirect_display_enabled{ redirect_display_enabled }
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

        py::module context_module = get_request_context_module();
    }

    namespace
    {
        class splinter_cell
        {
        public:

            splinter_cell()
            {
                auto null_out = py::cpp_function([](const std::string&) {});

                py::module sys = py::module::import("sys");
                m_stdout_func = sys.attr("stdout").attr("write");
                m_stderr_func = sys.attr("stderr").attr("write");
                sys.attr("stdout").attr("write") = null_out;
                sys.attr("stderr").attr("write") = null_out;
            }

            ~splinter_cell()
            {
                py::module sys = py::module::import("sys");
                sys.attr("stdout").attr("write") = m_stdout_func;
                sys.attr("stderr").attr("write") = m_stderr_func;
            }

        private:

            py::object m_stdout_func;
            py::object m_stderr_func;
        };
    }

    void raw_interpreter::execute_request_impl(
        send_reply_callback cb,
        int execution_count,
        const std::string& code,
        xeus::execute_request_config config,
        nl::json /*user_expressions*/)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
        py::str code_copy;
        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(config.allow_stdin);
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
            if (py::isinstance(last_stmt, ast.attr("Expr")) && !config.silent)
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
                splinter_cell guard;
                py::object compiled_code = builtins.attr("compile")(code_ast, filename, "exec");
                exec(compiled_code);
            }

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

            if (!config.silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            cb(xeus::create_error_reply(error.m_ename, error.m_evalue, error.m_traceback));
            return;
        }

        // Cache inputs
        py::globals()["_iii"] = py::globals()["_ii"];
        py::globals()["_ii"] = py::globals()["_i"];
        py::globals()["_i"] = code;

        kernel_res = xeus::create_successful_reply(execution_count);

        cb(kernel_res);
    }

    nl::json raw_interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        py::gil_scoped_acquire acquire;
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

        return xeus::create_complete_reply(matches, cursor_start, cursor_pos, nl::json::object());
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

        return xeus::create_inspect_reply(found, pub_data, nl::json::object());
    }

    nl::json raw_interpreter::is_complete_request_impl(const std::string&)
    {
        return xeus::create_is_complete_reply("complete", "");
    }

    nl::json raw_interpreter::kernel_info_request_impl()
    {

        /* The jupyter-console banner for xeus-python is the following:
          __  _____ _   _ ___
          \ \/ / _ \ | | / __|
           >  <  __/ |_| \__ \
          /_/\_\___|\__,_|___/

          xeus-python: a Jupyter kernel for Python
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

        nl::json help_links = nl::json::array();
        help_links.push_back({
            {"text", "Xeus-Python Reference"},
            {"url", "https://xeus-python.readthedocs.io"}
        });

        return xeus::create_info_reply(
            "5.3",                 // protocol_version
            "xeus-python",      // implementation
            XPYT_VERSION,       // implementation_version
            "python",           // language_name
            PY_VERSION,         // language_version
            "text/x-python",    // language_mimetype
            ".py",              // language_file_extension
            "ipython" + std::to_string(PY_MAJOR_VERSION), // pygments_lexer
            R"({"name": "ipython", "version": )" + std::to_string(PY_MAJOR_VERSION) + "}",    // language_codemirror_mode
            "python",           // language_nbconvert_exporter
            banner,             // banner
            false,              // debugger
            help_links          // help_links
        );
    }

    void raw_interpreter::shutdown_request_impl()
    {
    }

    void raw_interpreter::set_request_context(xeus::xrequest_context context)
    {
        py::gil_scoped_acquire acquire;
        py::module context_module = get_request_context_module();
        context_module.attr("set_request_context")(context);
    }

    const xeus::xrequest_context& raw_interpreter::get_request_context() const noexcept
    {
        py::gil_scoped_acquire acquire;
        py::module context_module = get_request_context_module();
        py::object res = context_module.attr("get_request_context")();
        return *(res.cast<xeus::xrequest_context*>());
    }

    void raw_interpreter::redirect_output()
    {
        py::module sys = py::module::import("sys");
        py::module stream_module = get_stream_module();

        sys.attr("stdout") = stream_module.attr("Stream")("stdout");
        sys.attr("stderr") = stream_module.attr("Stream")("stderr");
    }

}
