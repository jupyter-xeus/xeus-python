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

        m_ipython_shell_app.attr("initialize")();
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
        nl::json kernel_res;

        // Reset traceback
        m_ipython_shell.attr("last_error") = py::none();

        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(config.allow_stdin);

        bool exception_occurred = false;
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
            kernel_res["ename"] = "std::runtime_error";
            kernel_res["evalue"] = error_msg;
            exception_occurred = true;
        }
        catch(...)
        {
            if(!config.silent)
            {
                publish_execution_error("unknown_error", "", std::vector<std::string>());
            }
            kernel_res["ename"] = "UnknownError";
            kernel_res["evalue"] = "";
            exception_occurred = true;
        }

        // Get payload
        kernel_res["payload"] = m_ipython_shell.attr("payload_manager").attr("read_payload")();
        m_ipython_shell.attr("payload_manager").attr("clear_payload")();

        if(exception_occurred)
        {
            kernel_res["status"] = "error";
            kernel_res["traceback"] = std::vector<std::string>();
            cb(kernel_res);
            return;
        }

        if (m_ipython_shell.attr("last_error").is_none())
        {
            kernel_res["status"] = "ok";
            kernel_res["user_expressions"] = m_ipython_shell.attr("user_expressions")(user_expressions);
        }
        else
        {
            py::list pyerror = m_ipython_shell.attr("last_error");

            xerror error = extract_error(pyerror);

            if (!config.silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            kernel_res["status"] = "error";
            kernel_res["ename"] = error.m_ename;
            kernel_res["evalue"] = error.m_evalue;
            kernel_res["traceback"] = error.m_traceback;
        }
        cb(kernel_res);
    }

    nl::json interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;

        py::list completion = m_ipython_shell.attr("complete_code")(code, cursor_pos);

        kernel_res["matches"] = completion[0];
        kernel_res["cursor_start"] = completion[1];
        kernel_res["cursor_end"] = completion[2];
        kernel_res["metadata"] = nl::json::object();
        kernel_res["status"] = "ok";

        return kernel_res;
    }

    nl::json interpreter::inspect_request_impl(const std::string& code,
                                               int cursor_pos,
                                               int detail_level)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;
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

        kernel_res["data"] = data;
        kernel_res["metadata"] = nl::json::object();
        kernel_res["found"] = found;
        kernel_res["status"] = "ok";
        return kernel_res;
    }

    nl::json interpreter::is_complete_request_impl(const std::string& code)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;

        py::object transformer_manager = py::getattr(m_ipython_shell, "input_transformer_manager", py::none());
        if (transformer_manager.is_none())
        {
            transformer_manager = m_ipython_shell.attr("input_splitter");
        }

        py::list result = transformer_manager.attr("check_complete")(code);
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

        // Reset traceback
        m_ipython_shell.attr("last_error") = py::none();

        try
        {
            exec(py::str(code));

            reply["status"] = "ok";
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

            reply["status"] = "error";
            reply["ename"] = error.m_ename;
            reply["evalue"] = error.m_evalue;
            reply["traceback"] = error.m_traceback;
        }

        return reply;
    }

    void interpreter::set_request_context(xeus::xrequest_context context)
    {
        py::gil_scoped_acquire acquire;
        py::module context_module = get_request_context_module();
        context_module.attr("set_request_context")(context);
    }

    const xeus::xrequest_context& interpreter::get_request_context() const noexcept
    {
        py::gil_scoped_acquire acquire;
        py::module context_module = get_request_context_module();
        py::object res = context_module.attr("get_request_context")();
        return *(res.cast<xeus::xrequest_context*>());
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

}
