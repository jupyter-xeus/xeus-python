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

#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/eval.h"

#include "xeus-python/xutils.hpp"
#include "xcomm.hpp"

#include "xkernel.hpp"
#include "xinternal_utils.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt_ipython
{
    /***********************
     * xkernel declaration *
     **********************/

    struct xkernel
    {
        xkernel() = default;

        py::dict get_parent();

        py::object m_comm_manager;
    };

    /**************************
     * xkernel implementation *
     **************************/

    py::dict xkernel::get_parent()
    {
        return py::dict(py::arg("header") = xeus::get_interpreter().parent_header().get<py::object>());
    }

    /*****************
     * kernel module *
     *****************/

    py::module get_kernel_module_impl()
    {
        py::module kernel_module = xpyt::create_module("kernel");

        py::class_<xkernel>(kernel_module, "XKernel")
            .def(py::init<>())
            .def("get_parent", &xkernel::get_parent)
            .def_property_readonly("_parent_header", &xkernel::get_parent)
            .def_readwrite("comm_manager", &xkernel::m_comm_manager);

        return kernel_module;
    }
}


namespace xpyt_raw
{

    struct xmock_ipython
    {
        py::object kernel;
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
            return py::dict(py::arg("header") = xeus::get_interpreter().parent_header().get<py::object>());
        }

        xpyt::xcomm_manager m_comm_manager;
    };

    /*****************
     * kernel module *
     *****************/

    void bind_history_manager(py::module& kernel_module)
    {
        py::class_<xeus::xhistory_manager>(kernel_module, "HistoryManager")
            .def_property_readonly("session_number", [](xeus::xhistory_manager&) {return 0;})
            .def("get_range", [](xeus::xhistory_manager& me,
                int session,
                int start,
                int stop,
                bool raw,
                bool output)
                {
                    return me.get_range(session, start, stop, raw, output)["history"];
                },
                py::arg("session") = 0,
                py::arg("start") = 0,
                py::arg("stop") = 1000,
                py::arg("raw") = true,
                py::arg("output") = false)
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
                    py::arg("raw") = true,
                    py::arg("output") = false)
            .def("get_tail",
                [](const xeus::xhistory_manager& me, int last_n, bool raw, bool output)
                {
                    return me.get_tail(last_n, raw, output)["history"];
                },
                py::arg("last_n"),
                    py::arg("raw") = true,
                    py::arg("output") = false
                    )
                    .def("search",
                        [](const xeus::xhistory_manager& me, std::string pattern, bool raw, bool output, py::object py_n, bool unique)
                        {
                            int n = py_n.is_none() ? 1000 : py::cast<int>(py_n);
                            return me.search(pattern, raw, output, n, unique)["history"];
                        },
                        py::arg("pattern") = "*",
                            py::arg("raw") = true,
                            py::arg("output") = false,
                            py::arg("n") = py::none(),
                            py::arg("unique") = false);
    }


    void bind_comm(py::module& kernel_module)
    {
        py::class_<xpyt::xcomm>(kernel_module, "Comm")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&, py::kwargs>(),
                "target_name"_a="", "data"_a=py::dict(), "metadata"_a=py::dict(), "buffers"_a=py::list()
            )
            .def("close", &xpyt::xcomm::close, "data"_a=py::dict(), "metadata"_a=py::dict(), "buffers"_a=py::list())
            .def("send", &xpyt::xcomm::send, "data"_a=py::dict(), "metadata"_a=py::dict(), "buffers"_a=py::list())
            .def("on_msg", &xpyt::xcomm::on_msg)
            .def("on_close", &xpyt::xcomm::on_close)
            .def_property_readonly("comm_id", &xpyt::xcomm::comm_id)
            .def_property_readonly("kernel", &xpyt::xcomm::kernel);

        py::class_<xpyt::xcomm_manager>(kernel_module, "CommManager")
            .def(py::init<>())
            .def("register_target", &xpyt::xcomm_manager::register_target);
    }

    void bind_mock_objects(py::module& kernel_module)
    {
        py::class_<xmock_kernel>(kernel_module, "MockKernel", py::dynamic_attr())
            .def(py::init<>())
            .def_property_readonly("_parent_header", &xmock_kernel::parent_header)
            .def_readwrite("comm_manager", &xmock_kernel::m_comm_manager);

        py::class_<xmock_ipython>(kernel_module, "MockIPython")
            .def(py::init<>())
            .def_readwrite("kernel", &xmock_ipython::kernel)
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

                m_instance = kernel_module.attr("MockIPython")();
                m_instance.attr("kernel") = kernel_module.attr("MockKernel")();
            }
            return m_instance;
        }

    private:
        mutable py::object m_instance;
    };

    py::module get_kernel_module_impl()
    {
        py::module kernel_module = xpyt::create_module("kernel");

        bind_history_manager(kernel_module);
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
}


namespace xpyt
{
    py::module get_kernel_module(bool raw_mode /*false*/)
    {

        static py::module kernel_module;
        if (raw_mode)
        {
            kernel_module = xpyt_raw::get_kernel_module_impl();
        }
        else
        {

            kernel_module = xpyt_ipython::get_kernel_module_impl();
        }

        return kernel_module;
    }
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
