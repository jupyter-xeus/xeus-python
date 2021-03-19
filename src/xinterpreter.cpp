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
#include "xcompiler.hpp"
#include "xdisplay.hpp"
#include "xinput.hpp"
#include "xinternal_utils.hpp"
#include "xstream.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    interpreter::interpreter(bool redirect_output_enabled/*=true*/, bool redirect_display_enabled/*=true*/)
        : m_redirect_display_enabled{redirect_display_enabled}
    {
        xeus::register_interpreter(this);
        if (redirect_output_enabled)
        {
            redirect_output();
        }
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

        // Monkey patching "from ipykernel.comm import Comm"
        sys.attr("modules")["ipykernel.comm"] = get_comm_module();

        py::module display_module = get_display_module();
        py::module traceback_module = get_traceback_module();

        py::dict scope;
        scope["CommManager"] = get_comm_module().attr("CommManager");
        scope["set_last_error"] = traceback_module.attr("set_last_error");

        scope["XDisplayPublisher"] = display_module.attr("XDisplayPublisher");
        scope["XDisplayHook"] = display_module.attr("XDisplayHook");

        scope["XCachingCompiler"] = get_compiler_module().attr("XCachingCompiler");

        exec(py::str(R"(
import sys

from IPython.core.interactiveshell import InteractiveShell
from IPython.core.shellapp import InteractiveShellApp
from IPython.core.application import BaseIPythonApplication
from IPython.core import page, payloadpage


class XKernel():
    def __init__(self):
        self.comm_manager = CommManager()


class XPythonShell(InteractiveShell):
    def __init__(self, *args, **kwargs):
        super(XPythonShell, self).__init__(*args, **kwargs)
        self.kernel = XKernel()

    def enable_gui(self, gui=None):
        """Not implemented yet."""
        pass

    def init_hooks(self):
        super(XPythonShell, self).init_hooks()
        self.set_hook('show_in_pager', page.as_hook(payloadpage.page), 99)

    # Workaround for preventing IPython to show error traceback
    # We catch it and will display it later properly
    def showtraceback(self, exc_tuple=None, filename=None, tb_offset=None,
                      exception_only=False, running_compiled_code=False):
        try:
            etype, value, tb = self._get_exc_info(exc_tuple)
        except ValueError:
            print('No traceback available to show.', file=sys.stderr)
            return

        set_last_error(etype, value, tb)


class XPythonShellApp(BaseIPythonApplication, InteractiveShellApp):
    def initialize(self, argv=None):
        super(XPythonShellApp, self).initialize(argv)

        self.user_ns = {}

        # self.init_io() ?

        self.init_path()
        self.init_shell()

        self.init_extensions()
        self.init_code()

        sys.stdout.flush()
        sys.stderr.flush()

    def init_shell(self):
        self.shell = XPythonShell.instance(
            display_pub_class=XDisplayPublisher,
            displayhook_class=XDisplayHook,
            compiler_class=XCachingCompiler,
            user_ns=self.user_ns
        )

    # Overwrite exit logic, this is not part of the kernel protocole
    def exit(self, exit_status=0):
        pass
        )"), scope);

        m_ipython_shell_app = scope["XPythonShellApp"]();
        // TODO Pass argv to initialize
        m_ipython_shell_app.attr("initialize")();
        m_ipython_shell = m_ipython_shell_app.attr("shell");

        m_displayhook = m_ipython_shell.attr("displayhook");

        m_ipython_shell.attr("compile").attr("filename_mapper") = traceback_module.attr("register_filename_mapping");
    }

    nl::json interpreter::execute_request_impl(int /*execution_count*/,
                                               const std::string& code,
                                               bool silent,
                                               bool store_history,
                                               nl::json user_expressions,
                                               bool allow_stdin)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;

        py::module traceback = get_traceback_module();

        // Scope guard performing the temporary monkey patching of input and
        // getpass with a function sending input_request messages.
        auto input_guard = input_redirection(allow_stdin);

        py::object ipython_res = m_ipython_shell.attr("run_cell")(code, "store_history"_a=store_history, "silent"_a=silent);

        // Get payload
        kernel_res["payload"] = m_ipython_shell.attr("payload_manager").attr("read_payload")();
        m_ipython_shell.attr("payload_manager").attr("clear_payload")();

        if (traceback.attr("get_last_error")().is_none())
        {
            kernel_res["status"] = "ok";
            kernel_res["user_expressions"] = m_ipython_shell.attr("user_expressions")(user_expressions);
        }
        else
        {
            py::list pyerror = traceback.attr("get_last_error")();
            xerror error = extract_error(pyerror[0], pyerror[1], pyerror[2]);

            if (!silent)
            {
                publish_execution_error(error.m_ename, error.m_evalue, error.m_traceback);
            }

            kernel_res["status"] = "error";
            kernel_res["ename"] = error.m_ename;
            kernel_res["evalue"] = error.m_evalue;
            kernel_res["traceback"] = error.m_traceback;

            traceback.attr("reset_last_error")();
        }

        return kernel_res;
    }

    nl::json interpreter::complete_request_impl(
        const std::string& code,
        int cursor_pos)
    {
        py::gil_scoped_acquire acquire;
        nl::json kernel_res;

        py::module completer = py::module::import("IPython.core.completer");

        py::dict scope;
        scope["provisionalcompleter"] = completer.attr("provisionalcompleter");
        scope["rectify_completions"] = completer.attr("rectify_completions");
        scope["shell"] = m_ipython_shell;
        scope["code"] = code;
        scope["cursor_pos"] = cursor_pos;
        exec(py::str(R"(
with provisionalcompleter():
    raw_completions = shell.Completer.completions(code, cursor_pos)
    completions = list(rectify_completions(code, raw_completions))

    comps = []
    for comp in completions:
        comps.append(dict(
            start=comp.start,
            end=comp.end,
            text=comp.text,
            type=comp.type,
        ))

if completions:
    cursor_start = completions[0].start
    cursor_end = completions[0].end
    matches = [c.text for c in completions]
else:
    cursor_start = cursor_pos
    cursor_end = cursor_pos
    matches = []
        )"), scope);

        kernel_res["matches"] = scope["matches"];
        kernel_res["cursor_end"] = scope["cursor_end"];
        kernel_res["cursor_start"] = scope["cursor_start"];
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
        try
        {
            exec(py::str(code));

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

}
