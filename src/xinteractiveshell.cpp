#include "xinteractiveshell.hpp"
#include "xdisplay.hpp"

using namespace pybind11::literals;
namespace py = pybind11;

namespace xpyt
{

    void xinteractive_shell::init_magics()
    {
        m_magic_core = py::module::import("IPython.core.magic");
        m_magics_module = py::module::import("IPython.core.magics");
        m_extension_module = py::module::import("IPython.core.extensions");

        m_magics_manager = m_magic_core.attr("MagicsManager")("shell"_a=this);
        m_extension_manager = m_extension_module.attr("ExtensionManager")("shell"_a=this);

        //shell features required by extension manager
        m_builtin_trap = py::module::import("contextlib").attr("nullcontext")();
        m_ipython_dir = "";

        py::object osm_magics =  m_magics_module.attr("OSMagics");
        py::object basic_magics =  m_magics_module.attr("BasicMagics");
        py::object user_magics =  m_magics_module.attr("UserMagics");
        py::object extension_magics =  m_magics_module.attr("ExtensionMagics");
        m_magics_manager.attr("register")(osm_magics);
        m_magics_manager.attr("register")(basic_magics);
        m_magics_manager.attr("register")(user_magics);
        m_magics_manager.attr("register")(extension_magics);
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
           "load_ext"_a=line_magics["load_ext"]
        );
        cell_magics = py::dict(
            "writefile"_a=cell_magics["writefile"]);

        m_magics_manager.attr("magics") = py::dict(
           "line"_a=line_magics,
           "cell"_a=cell_magics);
    }


    xinteractive_shell::xinteractive_shell()
    {
        m_hooks = hooks_object();
        m_ipy_process = py::module::import("IPython.utils.process");
        m_db = py::dict();
        m_user_ns = py::dict("_dh"_a=py::list());
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

        if (magic_method.is_none()) {
            PyErr_SetString(PyExc_ValueError, "magics not found");
            throw py::error_already_set();
        }

        return magic_method(arg);

    }

    py::object xinteractive_shell::run_cell_magic(std::string name, std::string arg, std::string body)
    {
        py::object magic_method = m_magics_manager.attr("magics")["cell"].attr("get")(name);

        if (magic_method.is_none()) {
            PyErr_SetString(PyExc_ValueError, "cell magics not found");
            throw py::error_already_set();
        }

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

}
