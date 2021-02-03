#include "xinteractiveshell.hpp"

#include <iostream>
#include <string>
#include <utility>

#include "xeus/xinterpreter.hpp"
#include "xeus/xhistory_manager.hpp"

#include "pybind11/eval.h"

#include "xdisplay.hpp"
#include "xeus-python/xutils.hpp"
#include "xinspect.hpp"
#include "xnullcontext.hpp"

using namespace pybind11::literals;
namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    void xinteractive_shell::init_magics()
    {
        m_magic_core = py::module::import("IPython.core.magic");
        m_magics_module = py::module::import("IPython.core.magics");
        m_extension_module = py::module::import("IPython.core.extensions");

        m_magics_manager = m_magic_core.attr("MagicsManager")("shell"_a=this);
        m_extension_manager = m_extension_module.attr("ExtensionManager")("shell"_a=this);

        // Shell features required by extension manager
        m_builtin_trap = get_nullcontext_module().attr("nullcontext")();

        m_ipython_dir = "";
        m_pylab_gui_select = py::none();

        py::object osm_magics = m_magics_module.attr("OSMagics");
        py::object basic_magics = m_magics_module.attr("BasicMagics");
        py::object user_magics = m_magics_module.attr("UserMagics");
        py::object extension_magics = m_magics_module.attr("ExtensionMagics");
        py::object history_magics = m_magics_module.attr("HistoryMagics");
        py::object ns_magics = m_magics_module.attr("NamespaceMagics");
        py::object execution_magics = m_magics_module.attr("ExecutionMagics");
        py::object pylab_magics = m_magics_module.attr("PylabMagics");

        m_magics_manager.attr("register")(osm_magics);
        m_magics_manager.attr("register")(basic_magics);
        m_magics_manager.attr("register")(user_magics);
        m_magics_manager.attr("register")(extension_magics);
        m_magics_manager.attr("register")(history_magics);
        m_magics_manager.attr("register")(ns_magics);
        m_magics_manager.attr("register")(execution_magics);
        m_magics_manager.attr("register")(pylab_magics);

        m_magics_manager.attr("user_magics") = user_magics("shell"_a=this);

        //select magics supported by xeus-python
        auto line_magics = m_magics_manager.attr("magics")["line"];
        auto cell_magics = m_magics_manager.attr("magics")["cell"];

        line_magics = py::dict(
           "cd"_a=line_magics["cd"],
           "env"_a=line_magics["env"],
           "set_env"_a=line_magics["set_env"],
           "pwd"_a=line_magics["pwd"],
           "magic"_a=line_magics["magic"],
           "load_ext"_a=line_magics["load_ext"],
           "pushd"_a=line_magics["pushd"],
           "popd"_a=line_magics["popd"],
           "dirs"_a=line_magics["dirs"],
           "dhist"_a=line_magics["dhist"],
           "sx"_a=line_magics["sx"],
           "system"_a=line_magics["system"],
           "bookmark"_a=line_magics["bookmark"],
           //history magics
           "history"_a=line_magics["history"],
           "recall"_a=line_magics["recall"],
           "rerun"_a=line_magics["rerun"],
           //namespace magics
           "pinfo"_a=line_magics["pinfo"],
           //execution magics
           "timeit"_a=line_magics["timeit"],
           // matplotlib
           "matplotlib"_a=line_magics["matplotlib"]
        );

        cell_magics = py::dict(
            "writefile"_a=cell_magics["writefile"],
            "sx"_a=cell_magics["sx"],
            "system"_a=cell_magics["system"],
            //execution magics
            "timeit"_a=cell_magics["timeit"]
            );

        m_magics_manager.attr("magics") = py::dict(
           "line"_a=line_magics,
           "cell"_a=cell_magics);
    }

    xinteractive_shell::xinteractive_shell()
    {
        m_config = py::dict();
        p_history_manager = &xeus::get_interpreter().get_history_manager();
        m_hooks = hooks_object();
        m_ipy_process = py::module::import("IPython.utils.process");
        py::module os_module = py::module::import("os");
        m_db = py::dict();
        m_user_ns = py::dict("_dh"_a=py::list());
        m_dir_stack = py::list();
        m_home_dir = os_module.attr("path").attr("expanduser")("~");
        init_magics();
    }

    py::object xinteractive_shell::system(py::str cmd)
    {
        return m_ipy_process.attr("system")(cmd);
    }

    py::object xinteractive_shell::getoutput(py::str cmd)
    {
        auto stream = m_ipy_process.attr("getoutput")(cmd);
        return stream.attr("splitlines")();
    }

    py::object xinteractive_shell::run_line_magic(std::string name, std::string arg)
    {
        py::object magic_method = m_magics_manager
            .attr("magics")["line"]
            .attr("get")(name);

        if (magic_method.is_none())
        {
            PyErr_SetString(PyExc_ValueError, "magics not found");
            throw py::error_already_set();
        }

        // required by timeit magics (which uses user_ns as globals dict)
        m_user_ns.attr("update")(py::globals());

        return magic_method(arg);
    }

    py::object xinteractive_shell::run_cell_magic(std::string name, std::string arg, std::string body)
    {
        py::object magic_method = m_magics_manager.attr("magics")["cell"].attr("get")(name);

        if (magic_method.is_none())
        {
            PyErr_SetString(PyExc_ValueError, "cell magics not found");
            throw py::error_already_set();
        }

        // required by timeit magics (which uses user_ns as globals dict)
        m_user_ns.attr("update")(py::globals());

        return magic_method(arg, body);
    }

    void xinteractive_shell::register_magic_function(py::object func, std::string magic_kind, py::object magic_name)
    {
        m_magics_manager.attr("register_function")(
            func, "magic_kind"_a=magic_kind, "magic_name"_a=magic_name);
    }

    void xinteractive_shell::register_magics(py::args args)
    {
        m_magics_manager.attr("register")(*args);
    }

    // manage payloads
    // payloads are required by recall magic
    void xinteractive_shell::set_next_input(std::string s, bool replace)
    {
        nl::json data = nl::json::object({
            {"text", s},
            {"source", "set_next_input"},
            {"replace", replace}
        });

        m_payloads.push_back(std::move(data));
    }

    void xinteractive_shell::inspect(std::string, std::string oname, py::kwargs)
    {
        auto result = formatted_docstring(oname);
        nl::json data = nl::json::object({
            {"data", {
                {"text/plain", result}
            }},
            {"source", "page"},
            {"start", 0}
        });

        m_payloads.push_back(std::move(data));
    }

    py::tuple xinteractive_shell::enable_matplotlib(py::object gui)
    {
        py::module pt = py::module::import("IPython.core.pylabtools");
        py::tuple gui_backend_tuple = pt.attr("find_gui_and_backend")(gui, m_pylab_gui_select);
        // If we have our first gui selection, store it
        if (m_pylab_gui_select.is_none())
        {
            m_pylab_gui_select = gui;
        }
        // Otherwise if they are different
        else if (!gui.is(m_pylab_gui_select))
        {
            std::cerr << "Warning: Cannot change to a different GUI toolkit " << std::string(py::str(gui)) << "."
                        << " Using " << std::string(py::str(m_pylab_gui_select)) << " instead" << std::endl;
            gui_backend_tuple = pt.attr("find_gui_and_backend")(m_pylab_gui_select);
        }
        auto backend = gui_backend_tuple[1];
        pt.attr("activate_matplotlib")(backend);

        // Now we must activate the gui pylab wants to use, and fix %run to take
        // plot updates into account
        enable_gui(gui);

        // m_magics_manager.attr("registry")["ExecutionMagics"].attr("default_runner") = pt.mpl_runner(self.safe_execfile);

        return gui_backend_tuple;
    }

    void xinteractive_shell::clear_payloads()
    {
        m_payloads.clear();
    }

    const xinteractive_shell::payload_type & xinteractive_shell::get_payloads()
    {
        return m_payloads;
    }

    // run_cell required my %rerun magic
    void xinteractive_shell::run_cell(py::str code, bool)
    {
        // this is a placeholder for a real implementation
        // it does not handle multiple statements
        // nor magics parsing
        py::module builtins = py::module::import("builtins");
        std::string filename = "debug_this_thread";
        auto compiled_code = builtins.attr("compile")(code, filename, "single");

        // we need to pass user_ns because in a nested interpreter
        // we loose global namespace (results of previous evals)
        exec(compiled_code, m_user_ns);
    }

    void xinteractive_shell::ex(py::str cmd)
    {
        exec(cmd, m_user_ns);
    }

    py::object xinteractive_shell::ev(py::str expr)
    {
        return eval(expr, m_user_ns);
    }

    // define getters
    py::object xinteractive_shell::get_magics_manager() const
    {
        return m_magics_manager;
    }

    py::object xinteractive_shell::get_extension_manager() const
    {
        return m_extension_manager;
    }

    py::dict xinteractive_shell::get_db() const
    {
        return m_db;
    }

    py::dict xinteractive_shell::get_user_ns() const
    {
        return m_user_ns;
    }

    py::dict xinteractive_shell::get_user_global_ns() const
    {
        return py::globals();
    }

    py::object xinteractive_shell::get_builtin_trap() const
    {
        return m_builtin_trap;
    }

    py::str xinteractive_shell::get_ipython_dir() const
    {
        return m_ipython_dir;
    }

    hooks_object xinteractive_shell::get_hooks() const
    {
        return m_hooks;
    }

    py::dict xinteractive_shell::get_config() const
    {
        return m_config;
    }

    void xinteractive_shell::set_config(py::dict config)
    {
        m_config = config;
    }

    const xeus::xhistory_manager& xinteractive_shell::get_history_manager()
    {
        return *p_history_manager;
    }
}
