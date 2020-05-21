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

#include "xcomm.hpp"
#include "xutils.hpp"
#include "xdisplay.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;
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

    void register_target(const py::str& target_name, const py::object& callback)
    {
        auto target_callback = [&callback] (xeus::xcomm&& comm, const xeus::xmessage& msg) {
            XPYT_HOLDING_GIL(callback(xcomm(std::move(comm)), cppmessage_to_pymessage(msg)));
        };

        xeus::get_interpreter().comm_manager().register_comm_target(
            static_cast<std::string>(target_name), target_callback
        );
    }

    auto init_magics(py::object & shell) 
    {
        py::module magic_core = py::module::import("IPython.core.magic");
        py::module magics_module = py::module::import("IPython.core.magics");

        py::object magics_manager = magic_core.attr("MagicsManager")();
        shell.attr("magics_manager") = magics_manager;

        py::object osm_magics_inst =  magics_module.attr("OSMagics")();
        py::object basic_magics_inst =  magics_module.attr("BasicMagics")();
        osm_magics_inst.attr("shell") = shell;
        osm_magics_inst.attr("magics")["cell"] = py::dict(
            "writefile"_a=osm_magics_inst.attr("writefile"));
        osm_magics_inst.attr("magics")["line"] = py::dict(
            "cd"_a=osm_magics_inst.attr("cd"),
            "env"_a=osm_magics_inst.attr("env"),
            "set_env"_a=osm_magics_inst.attr("set_env"),
            "pwd"_a=osm_magics_inst.attr("pwd"));
        basic_magics_inst.attr("shell") = shell;
        basic_magics_inst.attr("magics")["cell"] = py::dict();
        basic_magics_inst.attr("magics")["line"] = py::dict(
            "magic"_a=basic_magics_inst.attr("magic"));
        magics_manager.attr("register")(osm_magics_inst);
        magics_manager.attr("register")(basic_magics_inst);
    }

    namespace detail
    {
        struct xmock_object
        {
        };

        struct hooks_object
        {
            hooks_object() {}
        };

    }

    struct xmock_kernel
    {
        xmock_kernel() {}
        inline py::object parent_header() const
        {
            return py::dict(py::arg("header")=xeus::get_interpreter().parent_header().get<py::object>());
        }
    };

    /*****************
     * kernel module *
     *****************/

    py::module get_kernel_module_impl()
    {
        py::module kernel_module = py::module("kernel");

        py::class_<detail::xmock_object> _Mock(kernel_module, "_Mock");
        py::class_<xcomm>(kernel_module, "Comm")
            .def(py::init<py::args, py::kwargs>())
            .def("close", &xcomm::close)
            .def("send", &xcomm::send)
            .def("on_msg", &xcomm::on_msg)
            .def("on_close", &xcomm::on_close)
            .def_property_readonly("comm_id", &xcomm::comm_id)
            .def_property_readonly("kernel", &xcomm::kernel);
        py::class_<xmock_kernel>(kernel_module, "mock_kernel", py::dynamic_attr())
            .def(py::init<>())
            .def_property_readonly("_parent_header", &xmock_kernel::parent_header);
        py::class_<detail::hooks_object> hooks(kernel_module, "Hooks");


        kernel_module.def("register_target", &register_target);
        kernel_module.def("register_post_execute", [](py::args, py::kwargs) {});
        kernel_module.def("enable_gui", [](py::args, py::kwargs) {});
        kernel_module.def("showtraceback", [](py::args, py::kwargs) {});
        kernel_module.def("show_in_pager", [](py::str data, py::kwargs) {xpyt::xdisplay(py::dict("text/plain"_a=data), {}, {}, py::dict(), py::none(), py::none(), false, true);});

        py::module ipy_process = py::module::import("IPython.utils.process");
        kernel_module.def("system", [ipy_process](py::str cmd) {ipy_process.attr("system")(cmd);});
        kernel_module.def("getoutput", [ipy_process](py::str cmd){return ipy_process.attr("getoutput")(cmd).attr("splitlines")();});


        kernel_module.def("run_line_magic", [_Mock](std::string name, std::string arg) {

            py::object magic_method = _Mock.attr("magics_manager").attr("magics")["line"].attr("get")(name);

            if (magic_method.is_none()) {
                PyErr_SetString(PyExc_ValueError, "magics not found");
                throw py::error_already_set();
            }

            auto result = magic_method(arg);
            return result;
    
            });

        kernel_module.def("run_cell_magic", [_Mock](std::string name, std::string line, std::string cell) {

            py::object magic_method = _Mock.attr("magics_manager").attr("magics")["cell"].attr("get")(name);

            if (magic_method.is_none()) {
                PyErr_SetString(PyExc_ValueError, "cell magics not found");
                throw py::error_already_set();
            }

            auto result = magic_method(line, cell);
            return result;
    
            });


        hooks.attr("show_in_pager") = kernel_module.attr("show_in_pager");

        kernel_module.def("get_ipython", [kernel_module]() {
            py::object kernel = kernel_module.attr("mock_kernel")();
            py::object comm_manager = kernel_module.attr("_Mock");
            comm_manager.attr("register_target") = kernel_module.attr("register_target");
            kernel.attr("comm_manager") = comm_manager;

            py::object xeus_python =  kernel_module.attr("_Mock");
            xeus_python.attr("register_post_execute") = kernel_module.attr("register_post_execute");
            xeus_python.attr("enable_gui") = kernel_module.attr("enable_gui");
            xeus_python.attr("showtraceback") = kernel_module.attr("showtraceback");
            xeus_python.attr("kernel") = kernel;
            xeus_python.attr("db") = py::dict();
            xeus_python.attr("hooks") = kernel_module.attr("Hooks");
            xeus_python.attr("user_ns") = py::dict("_dh"_a=py::list());
            xeus_python.attr("run_line_magic") = kernel_module.attr("run_line_magic");
            xeus_python.attr("run_cell_magic") = kernel_module.attr("run_cell_magic");
            xeus_python.attr("system") = kernel_module.attr("system");
            xeus_python.attr("getoutput") = kernel_module.attr("getoutput");
            init_magics(xeus_python);
            return xeus_python;
        });

        return kernel_module;
    }

    py::module get_kernel_module()
    {
        static py::module kernel_module = get_kernel_module_impl();
        return kernel_module;
    }
}
