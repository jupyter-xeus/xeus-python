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

#include "xeus-python/xinterpreter.hpp"
#include "xeus-python/xeus_python_config.hpp"
#include "xeus-python/xtraceback.hpp"
#include "xeus-python/xutils.hpp"

#include "xcomm.hpp"
#include "xkernel.hpp"
#include "xdisplay.hpp"
#include "xinput.hpp"
#include "xinternal_utils.hpp"
#include "xstream.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    interpreter::interpreter(bool redirect_output_enabled /*=true*/, bool redirect_display_enabled /*=true*/)
        : m_redirect_output_enabled{redirect_output_enabled}, m_redirect_display_enabled{redirect_display_enabled}
    {
        xeus::register_interpreter(this);
    }

    interpreter::~interpreter()
    {
    }

    void interpreter::configure_impl()
    {
        if (m_release_gil_at_startup)
        {
            // The GIL is not held by default by the interpreter, so every time we need to execute Python code we
            // will need to acquire the GIL
            m_release_gil = gil_scoped_release_ptr(new py::gil_scoped_release());
        }

        py::gil_scoped_acquire acquire;

        py::module sys = py::module::import("sys");
        py::module logging = py::module::import("logging");

        py::module display_module = get_display_module();
        py::module traceback_module = get_traceback_module();
        py::module stream_module = get_stream_module();
        py::module comm_module = get_comm_module();
        py::module kernel_module = get_kernel_module();

        // Old approach: ipykernel provides the comm
        sys.attr("modules")["ipykernel.comm"] = comm_module;
        // New approach: we provide our comm module
        sys.attr("modules")["comm"] = comm_module;

        instanciate_ipython_shell();

        py::dict modules = sys.attr("modules");
        std::vector<std::string> internal_mod_paths;
        internal_mod_paths.reserve(len(modules));

        for (auto item : modules)
        {
            py::object mod = py::reinterpret_borrow<py::object>(item.second);
            if (py::hasattr(mod, "__file__"))
            {
                std::string path = mod.attr("__file__").cast<std::string>();
                internal_mod_paths.push_back(path);
            }
        }


        m_ipython_shell_app.attr("initialize")(use_jedi_for_completion());
        m_ipython_shell = m_ipython_shell_app.attr("shell");

        // Setting kernel property owning the CommManager and get_parent
        m_ipython_shell.attr("kernel") = kernel_module.attr("XKernel")();
        m_ipython_shell.attr("kernel").attr("comm_manager") = comm_module.attr("CommManager")();

        // Initializing the DisplayPublisher
        m_ipython_shell.attr("display_pub").attr("publish_display_data") = display_module.attr("publish_display_data");
        m_ipython_shell.attr("display_pub").attr("clear_output") = display_module.attr("clear_output");

        // Initializing the DisplayHook
        m_displayhook = m_ipython_shell.attr("displayhook");
        m_displayhook.attr("publish_execution_result") = display_module.attr("publish_execution_result");

        // Needed for redirecting logging to the terminal
        m_logger = m_ipython_shell_app.attr("log");
        m_terminal_stream = stream_module.attr("TerminalStream")();
        m_logger.attr("handlers") = py::list(0);
        m_logger.attr("addHandler")(logging.attr("StreamHandler")(m_terminal_stream));

        // Initializing the compiler
        m_ipython_shell.attr("compile").attr("filename_mapper") = traceback_module.attr("register_filename_mapping");
        m_ipython_shell.attr("compile").attr("get_filename") = traceback_module.attr("get_filename");

        if (m_redirect_output_enabled)
        {
            redirect_output();
        }

        py::module context_module = get_request_context_module();
    }

    void interpreter::execute_request_impl(send_reply_callback cb,
                                           int /*execution_count*/,
                                           const std::string& code,
                                           xeus::execute_request_config config,
                                           nl::json user_expressions)
    {
        py::gil_scoped_acquire acquire;

        // Reset traceback
        m_ipython_shell.attr("last_error") = py::none();

        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(config.allow_stdin);

        bool exception_occurred = false;
        std::string ename;
        std::string evalue;
        std::vector<std::string> traceback;

        try
        {
            m_ipython_shell.attr("run_cell")(code, "store_history"_a=config.store_history, "silent"_a=config.silent);
        }
        catch(std::runtime_error& e)
        {
            const std::string error_msg = e.what();
            if(!config.silent)
            {
                publish_execution_error("RuntimeError", error_msg, std::vector<std::string>());
            }
            exception_occurred = true;
        }
        catch (py::error_already_set& e)
        {
            xerror error = extract_already_set_error(e);
            if (!config.silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            ename = error.m_ename;
            evalue = error.m_evalue;
            traceback = error.m_traceback;
            exception_occurred = true;
        }
        catch(...)
        {
            if(!config.silent)
            {
                publish_execution_error("unknown_error", "", std::vector<std::string>());
            }
            ename = "UnknownError";
            evalue = "";
            exception_occurred = true;
        }

        // Get payload
        nl::json payload = m_ipython_shell.attr("payload_manager").attr("read_payload")();
        m_ipython_shell.attr("payload_manager").attr("clear_payload")();

        if(exception_occurred)
        {
            cb(xeus::create_error_reply(ename, evalue, traceback));
            return;
        }

        if (m_ipython_shell.attr("last_error").is_none())
        {
            nl::json user_exprs = m_ipython_shell.attr("user_expressions")(user_expressions);
            cb(xeus::create_successful_reply(payload, user_exprs));
        }
        else
        {
            py::list pyerror = m_ipython_shell.attr("last_error");

            xerror error = extract_error(pyerror);

            if (!config.silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            cb(xeus::create_error_reply(error.m_ename, error.m_evalue, error.m_traceback));
        }
    }

    nl::json interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        py::gil_scoped_acquire acquire;

        py::list completion = m_ipython_shell.attr("complete_code")(code, cursor_pos);

        nl::json matches = completion[0];
        int cursor_start = completion[1].cast<int>();
        int cursor_end = completion[2].cast<int>();

        return xeus::create_complete_reply(matches, cursor_start, cursor_end, nl::json::object());
    }

    nl::json interpreter::inspect_request_impl(const std::string& code,
                                               int cursor_pos,
                                               int detail_level)
    {
        py::gil_scoped_acquire acquire;
        nl::json data = nl::json::object();
        bool found = false;

        py::module tokenutil = py::module::import("IPython.utils.tokenutil");
        py::str name = tokenutil.attr("token_at_cursor")(code, cursor_pos);

        try
        {
            data = m_ipython_shell.attr("object_inspect_mime")(
                name,
                "detail_level"_a=detail_level
            );
            found = true;
        }
        catch (py::error_already_set& e)
        {
            // pass
        }

        return xeus::create_inspect_reply(found, data, nl::json::object());
    }

    nl::json interpreter::is_complete_request_impl(const std::string& code)
    {
        py::gil_scoped_acquire acquire;

        py::object transformer_manager = py::getattr(m_ipython_shell, "input_transformer_manager", py::none());
        if (transformer_manager.is_none())
        {
            transformer_manager = m_ipython_shell.attr("input_splitter");
        }

        py::list result = transformer_manager.attr("check_complete")(code);
        std::string status = result[0].cast<std::string>();
        std::string indent;

        if (status == "incomplete")
        {
            indent = std::string(result[1].cast<std::size_t>(), ' ');
        }

        return xeus::create_is_complete_reply(status, indent);
    }

    nl::json interpreter::kernel_info_request_impl()
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

        bool has_debugger = (PY_MAJOR_VERSION != 3) || (PY_MAJOR_VERSION != 13);
        nl::json rep = xeus::create_info_reply(
            "5.5",              // protocol_version - overwrited in xeus core
            "xeus-python",      // implementation
            XPYT_VERSION,       // implementation_version
            "python",           // language_name
            PY_VERSION,         // language_version
            "text/x-python",    // language_mimetype
            ".py",              // language_file_extension
            "ipython" + std::to_string(PY_MAJOR_VERSION), // pygments_lexer
            R"({"name": "ipython", "version": )" + std::to_string(PY_MAJOR_VERSION) + "}",
            "python",           // language_nbconvert_exporter
            banner,             // banner
            has_debugger,       // debugger
            help_links          // help_links
        );
        // use a dict, string seems to be not supported by the frontend
        rep["language_info"]["codemirror_mode"] = nl::json::object({
            {"name", "ipython"},
            {"version", PY_MAJOR_VERSION}
        });
        if (has_debugger)
        {
            rep["supported_features"] = nl::json::array({"debugger"});
        }
        return rep;
    }

    void interpreter::shutdown_request_impl()
    {
    }

    nl::json interpreter::internal_request_impl(const nl::json& content)
    {
        py::gil_scoped_acquire acquire;
        std::string code = content.value("code", "");

        // Reset traceback
        m_ipython_shell.attr("last_error") = py::none();

        try
        {
            exec(py::str(code));
            return xeus::create_successful_reply();
        }
        catch (py::error_already_set& e)
        {
            // This will grab the latest traceback and set shell.last_error
            m_ipython_shell.attr("showtraceback")();

            py::list pyerror = m_ipython_shell.attr("last_error");

            xerror error = extract_error(pyerror);

            publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            error.m_traceback.resize(1);
            error.m_traceback[0] = code;

            return xeus::create_error_reply(error.m_ename, error.m_evalue, error.m_traceback);
        }
    }

    void interpreter::set_request_context(xeus::xrequest_context context)
    {
        py::gil_scoped_acquire acquire;
        py::module context_module = get_request_context_module();
        context_module.attr("set_request_context")(context);
    }

    namespace
    {
        xeus::xrequest_context empty_request_context{};
    }

    const xeus::xrequest_context& interpreter::get_request_context() const noexcept
    {
        py::gil_scoped_acquire acquire;
        py::module context_module = get_request_context_module();
        // When the debugger is started, it send some python code to execute, that triggers
        // a call to publish_stream, and ultimately to this function. However:
        // - we are out of the handling of an execute_request, therefore set_request_context
        // has not been called
        // - we cannot set it from another thread (the context of the context variable would
        // be different)
        // Therefore, we have to catch the exception thrown when the context variable is empty.
        try
        {
            py::object res = context_module.attr("get_request_context")();
            return *(res.cast<xeus::xrequest_context*>());
        }
        catch (py::error_already_set& e)
        {
            return empty_request_context;
        }
    }

    void interpreter::redirect_output()
    {
        py::module sys = py::module::import("sys");
        py::module stream_module = get_stream_module();

        sys.attr("stdout") = stream_module.attr("Stream")("stdout");
        sys.attr("stderr") = stream_module.attr("Stream")("stderr");
    }

    void interpreter::instanciate_ipython_shell()
    {
        m_ipython_shell_app = py::module::import("xeus_python_shell.shell").attr("XPythonShellApp")();
    }

    bool interpreter::use_jedi_for_completion() const
    {
        return true;
    }
}
