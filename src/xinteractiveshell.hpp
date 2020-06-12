#include "pybind11/pybind11.h"
#include "xdisplay.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
using namespace pybind11::literals;

namespace xpyt 
{

    struct hooks_object
    {
        static inline void show_in_pager(py::str data, py::kwargs)
        {
            xdisplay(py::dict("text/plain"_a=data), {}, {}, py::dict(), py::none(), py::none(), false, true);
        }
    };

    class xinteractive_shell
    {

    public:

        // default constructor
        xinteractive_shell();

        // mock required methods
        void register_post_execute(py::args, py::kwargs) {};
        void enable_gui(py::args, py::kwargs) {};
        void observe(py::args, py::kwargs) {};
        void showtraceback(py::args, py::kwargs) {};


        // run system commands
        py::object system(py::str cmd);
        py::object getoutput(py::str cmd); 

        // run magics
        py::object run_line_magic(std::string name, std::string arg);
        py::object run_cell_magic(std::string name, std::string arg, std::string body);
         
        // register magics
        void register_magic_function(py::object func, std::string magic_kind, py::object magic_name);
        void register_magics(py::args args);

        // public getters
        py::object get_magics_manager() const;
        py::object get_extension_manager() const;
        py::dict get_db() const;
        py::dict get_user_ns() const;
        py::object get_builtin_trap() const;
        py::str get_ipython_dir() const;
        hooks_object get_hooks() const;

    private:
        py::module m_ipy_process;
        py::module m_magic_core;
        py::module m_magics_module;
        py::module m_extension_module;

        py::object m_magics_manager;
        py::object m_extension_manager; // required by %load_ext

        // required by cd magic and others
        py::dict m_db;
        py::dict m_user_ns;

        // required by extension_manager
        py::object m_builtin_trap;
        py::str m_ipython_dir;

        hooks_object m_hooks;

        void init_magics();
    };
};
