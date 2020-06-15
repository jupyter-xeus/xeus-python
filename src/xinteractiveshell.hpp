#include <vector>
#include "nlohmann/json.hpp"
#include "pybind11/pybind11.h"
#include "xdisplay.hpp"

#include "xeus/xhistory_manager.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wattributes"
#endif


namespace nl = nlohmann;
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

        // required by history magics
        void set_next_input(std::string s, bool replace);
        void run_cell(py::str code, bool store_history);

        // required by pinfo
        void inspect(std::string, std::string oname, py::kwargs);

        // public getters
        py::object get_magics_manager() const;
        py::object get_extension_manager() const;
        py::dict get_db() const;
        py::dict get_user_ns() const;
        py::object get_builtin_trap() const;
        py::str get_ipython_dir() const;
        hooks_object get_hooks() const;

        py::list get_dir_stack() const { return m_dir_stack;};
        py::str get_home_dir() const { return m_home_dir;};

        const xeus::xhistory_manager & get_history_manager();

        // payload
        void clear_payloads();
        using payload_type = std::vector<nl::json>;
        const payload_type & get_payloads(); //pure C++ not exposed to Python

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

        // pager, required by %magics
        hooks_object m_hooks;

        // required by pushd
        py::list m_dir_stack;
        py::str m_home_dir;

        void init_magics();

        // history manager
        const xeus::xhistory_manager * p_history_manager;

        // store jupyter message protocol payloads
        payload_type m_payloads;

    };
};
