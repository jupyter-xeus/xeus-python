/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>
#include <utility>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"
#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/eval.h"

#include "xeus-python/xutils.hpp"

#include "xpython_kernel.hpp"
#include "xinteractiveshell.hpp"
#include "xinternal_utils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    /*********************
     * xcomm declaration *
     ********************/

    class xcomm
    {
    public:

        using python_callback_type = std::function<void(py::object)>;
        using cpp_callback_type = std::function<void(const xeus::xmessage&)>;
        using zmq_buffers_type = std::vector<zmq::message_t>;

        xcomm(const py::args& args, const py::kwargs& kwargs);
        xcomm(xeus::xcomm&& comm);
        xcomm(xcomm&& comm) = default;
        virtual ~xcomm();

        std::string comm_id() const;
        bool kernel() const;

        void close(const py::args& args, const py::kwargs& kwargs);
        void send(const py::args& args, const py::kwargs& kwargs);
        void on_msg(const python_callback_type& callback);
        void on_close(const python_callback_type& callback);

    private:

        xeus::xtarget* target(const py::kwargs& kwargs) const;
        xeus::xguid id(const py::kwargs& kwargs) const;
        cpp_callback_type cpp_callback(const python_callback_type& callback) const;

        xeus::xcomm m_comm;
    };

    struct xcomm_manager
    {
        xcomm_manager() = default;

        void register_target(const py::str& target_name, const py::object& callback);
    };

    /************************
     * xcomm implementation *
     ************************/

    xcomm::xcomm(const py::args& /*args*/, const py::kwargs& kwargs)
        : m_comm(target(kwargs), id(kwargs))
    {
        m_comm.open(
            kwargs.attr("get")("metadata", py::dict()),
            kwargs.attr("get")("data", py::dict()),
            pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()))
        );
    }

    xcomm::xcomm(xeus::xcomm&& comm)
        : m_comm(std::move(comm))
    {
    }

    xcomm::~xcomm()
    {
    }

    std::string xcomm::comm_id() const
    {
        return m_comm.id();
    }

    bool xcomm::kernel() const
    {
        return true;
    }

    void xcomm::close(const py::args& /*args*/, const py::kwargs& kwargs)
    {
        m_comm.close(
            kwargs.attr("get")("metadata", py::dict()),
            kwargs.attr("get")("data", py::dict()),
            pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()))
        );
    }

    void xcomm::send(const py::args& /*args*/, const py::kwargs& kwargs)
    {
        m_comm.send(
            kwargs.attr("get")("metadata", py::dict()),
            kwargs.attr("get")("data", py::dict()),
            pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()))
        );
    }

    void xcomm::on_msg(const python_callback_type& callback)
    {
        m_comm.on_message(cpp_callback(callback));
    }

    void xcomm::on_close(const python_callback_type& callback)
    {
        m_comm.on_close(cpp_callback(callback));
    }

    xeus::xtarget* xcomm::target(const py::kwargs& kwargs) const
    {
        std::string target_name = kwargs["target_name"].cast<std::string>();
        return xeus::get_interpreter().comm_manager().target(target_name);
    }

    xeus::xguid xcomm::id(const py::kwargs& kwargs) const
    {
        if (py::hasattr(kwargs, "comm_id"))
        {
            // TODO: prevent copy
            return xeus::xguid(kwargs["comm_id"].cast<std::string>());
        }
        else
        {
            return xeus::new_xguid();
        }
    }

    auto xcomm::cpp_callback(const python_callback_type& py_callback) const -> cpp_callback_type
    {
        return [this, py_callback](const xeus::xmessage& msg) {
            XPYT_HOLDING_GIL(py_callback(cppmessage_to_pymessage(msg)))
        };
    }

    void xcomm_manager::register_target(const py::str& target_name, const py::object& callback)
    {
        auto target_callback = [&callback] (xeus::xcomm&& comm, const xeus::xmessage& msg) {
            XPYT_HOLDING_GIL(callback(xcomm(std::move(comm)), cppmessage_to_pymessage(msg)));
        };

        xeus::get_interpreter().comm_manager().register_comm_target(
            static_cast<std::string>(target_name), target_callback
        );
    }

    /****************
     * mock objects *
     ****************/

    namespace detail
    {
        struct compiler_object
        {
            py::module builtins;

            py::object compile(py::object source, py::str filename, py::str mode, int flags=0)
            {
                return builtins.attr("compile")(source, filename, mode, flags);
            }

            compiler_object()
            {
               builtins = py::module::import("builtins");
            }

            py::object operator()(py::object source, py::str filename, py::str mode)
            {
                return compile(source, filename, mode, 0);
            }

            py::object ast_parse(
                py::str source,
                py::str filename="<unknown>",
                py::str symbol="exec")
            {
                 auto ast =  py::module::import("ast");
                 return compile(source, filename, symbol, py::cast<int>(ast.attr("PyCF_ONLY_AST")));
            }
        };
    }

    struct xmock_ipython
    {
        void register_post_execute(py::args, py::kwargs) {};
        void enable_gui(py::args, py::kwargs) {};
        void observe(py::args, py::kwargs) {};
        void showtraceback(py::args, py::kwargs) {};
    };

    struct xmock_kernel
    {
        xmock_kernel() = default;

        inline py::object parent_header() const
        {
            return py::dict(py::arg("header")=xeus::get_interpreter().parent_header().get<py::object>());
        }

        xcomm_manager m_comm_manager;
    };

    /*****************
     * kernel module *
     *****************/

    void bind_history_manager(py::module& kernel_module)
    {
        py::class_<xeus::xhistory_manager>(kernel_module, "HistoryManager")
            .def_property_readonly("session_number", [](xeus::xhistory_manager&){return 0;})
            .def("get_range", [](xeus::xhistory_manager& me,
                                 int session,
                                 int start,
                                 int stop,
                                 bool raw,
                                 bool output) {return me.get_range(session, start, stop, raw, output)["history"];},
                 py::arg("session")=0,
                 py::arg("start")=0,
                 py::arg("stop")=1000,
                 py::arg("raw")=true,
                 py::arg("output")=false)
            .def("get_range_by_str",
                [](const xeus::xhistory_manager& me, py::str range_str, bool raw, bool output)
                {
                    py::list range_split = range_str.attr("split")("-");
                    int start = std::stoi(py::cast<std::string>(range_split[0]));
                    int stop = (range_split.size() > 1) ? std::stoi(py::cast<std::string>(range_split[1])) : start + 1;
                    int session = 0;
                    return me.get_range(session, start - 1, stop - 1, raw, output)["history"];
                },
                py::arg("range_str"),
                py::arg("raw")=true,
                py::arg("output")=false)
            .def("get_tail",
                [](const xeus::xhistory_manager& me, int last_n, bool raw, bool output)
                {
                    return me.get_tail(last_n, raw, output)["history"];
                },
                py::arg("last_n"),
                py::arg("raw")=true,
                py::arg("output")=false
            )
            .def("search",
                [](const xeus::xhistory_manager& me, std::string pattern, bool raw, bool output, py::object py_n, bool unique)
                {
                    int n = py_n.is_none() ? 1000 : py::cast<int>(py_n);
                    return me.search(pattern, raw, output, n, unique)["history"];
                },
                py::arg("pattern")="*",
                py::arg("raw")=true,
                py::arg("output")=false,
                py::arg("n") = py::none(),
                py::arg("unique")=false);
    }

    void bind_interactive_shell(py::module& kernel_module)
    {
        // define compiler class for timeit magic
        py::class_<detail::compiler_object> Compiler(kernel_module, "Compiler");
        Compiler.def(py::init<>())
            .def("__call__", &detail::compiler_object::operator(), py::is_operator())
            .def("ast_parse", &detail::compiler_object::ast_parse,
                 py::arg("code"),
                 py::arg("filename")="<unknown>",
                 py::arg("symbol")="exec");

        py::class_<xinteractive_shell>(kernel_module, "XInteractiveShell", py::dynamic_attr())
            .def(py::init<>())
            .def_property_readonly("magics_manager", &xinteractive_shell::get_magics_manager)
            .def_property_readonly("extension_manager", &xinteractive_shell::get_extension_manager)
            .def_property_readonly("hooks", &xinteractive_shell::get_hooks)
            .def_property_readonly("db", &xinteractive_shell::get_db)
            .def_property_readonly("user_ns", &xinteractive_shell::get_user_ns)
            .def_property_readonly("user_global_ns", &xinteractive_shell::get_user_global_ns)
            .def_property_readonly("builtin_trap", &xinteractive_shell::get_builtin_trap)
            .def_property_readonly("ipython_dir", &xinteractive_shell::get_ipython_dir)
            .def_property_readonly("dir_stack", &xinteractive_shell::get_dir_stack)
            .def_property_readonly("home_dir", &xinteractive_shell::get_home_dir)
            .def_property_readonly("history_manager", &xinteractive_shell::get_history_manager)
            .def("run_line_magic", &xinteractive_shell::run_line_magic)
            .def("run_cell_magic", &xinteractive_shell::run_cell_magic)
            // magic method is deprecated but some magic functions still use it
            .def("magic", &xinteractive_shell::run_line_magic, "name"_a, "arg"_a="")
            .def("system", &xinteractive_shell::system)
            .def("getoutput", &xinteractive_shell::getoutput)
            .def("register_post_execute", &xinteractive_shell::register_post_execute)
            .def("enable_gui", &xinteractive_shell::enable_gui)
            .def("showtraceback", &xinteractive_shell::showtraceback)
            .def("observe", &xinteractive_shell::observe)
            // for timeit timeit
            .def("transform_cell", [](xinteractive_shell &, py::str raw_cell){return raw_cell;})
            .def("transform_ast", [](xinteractive_shell &, py::object ast){return ast;})
            // for matplotlib
            .def("enable_matplotlib", &xinteractive_shell::enable_matplotlib)
            // for pinfo (?magic)
            .def("_inspect", &xinteractive_shell::inspect)
            // generic magics code
            .def("run_cell", &xinteractive_shell::run_cell,
                py::arg("code"),
                py::arg("store_history")=false)
            .def("ex", &xinteractive_shell::ex, py::arg("cmd"))
            .def("ev", &xinteractive_shell::ev, py::arg("expr"))
            .def("register_magic_function",
                &xinteractive_shell::register_magic_function,
                "Register magic function",
                py::arg("func"),
                py::arg("magic_kind")="line",
                py::arg("magic_name")=py::none())
            .def("register_magics", &xinteractive_shell::register_magics)
            .def("set_next_input", &xinteractive_shell::set_next_input,
                 py::arg("text"),
                 py::arg("replace")=false)
            .attr("compile") = Compiler();
    }

    void bind_comm(py::module& kernel_module)
    {
        py::class_<xcomm>(kernel_module, "Comm")
            .def(py::init<py::args, py::kwargs>())
            .def("close", &xcomm::close)
            .def("send", &xcomm::send)
            .def("on_msg", &xcomm::on_msg)
            .def("on_close", &xcomm::on_close)
            .def_property_readonly("comm_id", &xcomm::comm_id)
            .def_property_readonly("kernel", &xcomm::kernel);

        py::class_<xcomm_manager>(kernel_module, "CommManager")
            .def(py::init<>())
            .def("register_target", &xcomm_manager::register_target);
    }

    void bind_mock_objects(py::module& kernel_module)
    {
        py::class_<xmock_kernel>(kernel_module, "MockKernel", py::dynamic_attr())
            .def(py::init<>())
            .def_property_readonly("_parent_header", &xmock_kernel::parent_header)
            .def_readwrite("comm_manager", &xmock_kernel::m_comm_manager);

        py::class_<xmock_ipython>(kernel_module, "MockIPython")
            .def("register_post_execute", &xmock_ipython::register_post_execute)
            .def("enable_gui", &xmock_ipython::enable_gui)
            .def("observe", &xmock_ipython::observe)
            .def("showtraceback", &xmock_ipython::showtraceback);
    }

    struct ipython_instance
    {
        ipython_instance() : m_instance(py::none())
        {
        }

        py::object get_instance(const py::module& kernel_module) const
        {
            if (m_instance.is(py::none()))
            {
                // The first import of IPython will throw if IPython has not been installed.
                // In this case we fallback on the mock_ipython object.
                try
                {
                    py::module::import("IPython.core.interactiveshell").attr("InteractiveShellABC").attr("register")(
                            kernel_module.attr("XInteractiveShell"));
                    m_instance = kernel_module.attr("XInteractiveShell")();
                    kernel_module.attr("has_ipython") = py::bool_(true);
                }
                catch(...)
                {
                    m_instance = kernel_module.attr("MockIPython");
                    kernel_module.attr("has_ipython") = py::bool_(false);
                }
                m_instance.attr("kernel") = kernel_module.attr("MockKernel")();
            }
            return m_instance;
        }

    private:

        mutable py::object m_instance;
    };

    py::module get_kernel_module_impl()
    {
        py::module kernel_module = create_module("kernel");

        py::class_<hooks_object>(kernel_module, "Hooks")
            .def_static("show_in_pager", &hooks_object::show_in_pager);

        bind_history_manager(kernel_module);
        bind_interactive_shell(kernel_module);
        bind_comm(kernel_module);
        bind_mock_objects(kernel_module);

        // To keep ipywidgets working, we must not import any module from IPython
        // before the kernel module has been defined and IPython.core has been
        // monkey patched. Otherwise, any call to register_target_comm will execute
        // that of IPython instead of that of xeus. Thereafter any call to register_comm
        // will execute that of xeus, where the target has not been registered, resulting
        // in a segmentation fault.
        // Initializing the xeus_python object as a memoized variable ensures the initialization
        // of the interactive shell (which imports a lot of module from IPython) will
        // occur AFTER IPython.core has been monkey_patched.
        // Notice that using a static variable in the lambda to achieve the memoization
        // results in a random crash at kernel shutdown.
        // Also notice that using an attribute of kernel_module to memoize results
        // in random segfault in the interpreter.
        ipython_instance ipyinstance;
        kernel_module.def("get_ipython", [ipyinstance, kernel_module]() {
            return ipyinstance.get_instance(kernel_module);
        });

        return kernel_module;
    }

    py::module get_kernel_module()
    {
        static py::module kernel_module = get_kernel_module_impl();
        return kernel_module;
    }
}
