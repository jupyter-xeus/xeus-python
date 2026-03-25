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

#include "xcomm.hpp"
#include "xinternal_utils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    /************************
     * xcomm implementation *
     ************************/

    xcomm::xcomm(const py::object& target_name, const py::object& data, const py::object& metadata, const py::object& buffers, const py::kwargs& kwargs)
        : m_comm(target(target_name), id(kwargs))
    {
        m_comm.open(metadata, data, pylist_to_cpp_buffers(buffers));
    }

    xcomm::xcomm(xeus::xcomm&& comm)
        : m_comm(std::move(comm))
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

    void xcomm::close(const py::object& data, const py::object& metadata, const py::object& buffers)
    {
        m_comm.close(metadata, data, pylist_to_cpp_buffers(buffers));
    }

    void xcomm::send(const py::object& data, const py::object& metadata, const py::object& buffers)
    {
        m_comm.send(metadata, data, pylist_to_cpp_buffers(buffers));
    }

    void xcomm::on_msg(const python_callback_type& callback)
    {
        m_comm.on_message(cpp_callback(callback));
    }

    void xcomm::on_close(const python_callback_type& callback)
    {
        m_comm.on_close(cpp_close_callback(callback));
    }

    void xcomm::on_close_cleanup(close_callback_type callback)
    {
        m_close_callback = std::move(callback);
        m_comm.on_close([this](const xeus::xmessage&)
        {
            m_close_callback(comm_id());
        });
    }

    const xeus::xtarget* xcomm::target(const py::object& target_name) const
    {
        auto& comm_manager = xeus::get_interpreter().comm_manager();
        auto res = comm_manager.target(target_name.cast<std::string>());
        if (!res)
        {
            comm_manager.register_comm_target(
                target_name.cast<std::string>(), [](xeus::xcomm&&, const xeus::xmessage&) {}
            );
        }
        return comm_manager.target(target_name.cast<std::string>());
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
        return [this, py_callback](const xeus::xmessage& msg)
        {
            XPYT_HOLDING_GIL(py_callback(cppmessage_to_pymessage(msg)))
        };
    }

    auto xcomm::cpp_close_callback(const python_callback_type& py_callback) const -> cpp_callback_type
    {
        return [this, py_callback](const xeus::xmessage& msg)
        {
            XPYT_HOLDING_GIL(py_callback(cppmessage_to_pymessage(msg)))
            if (m_close_callback)
            {
                m_close_callback(comm_id());
            }
        };
    }

    void xcomm_manager::register_target(const py::str& target_name, const py::object& callback)
    {
        auto target_callback = [callback] (xeus::xcomm&& comm, const xeus::xmessage& msg)
        {
            XPYT_HOLDING_GIL(callback(xcomm(std::move(comm)), cppmessage_to_pymessage(msg)));
        };

        xeus::get_interpreter().comm_manager().register_comm_target(
            static_cast<std::string>(target_name), target_callback
        );
    }

    void xcomm_manager::register_comm(py::object pycomm)
    {
        // pycomm was created with initial refcount = 0
        // Therefore we need to increment it to avoid its
        // deletion by the garbage collector
        pycomm.inc_ref();
        xcomm& comm = pycomm.cast<xcomm&>();
        comm.on_close_cleanup([this](std::string id)
        {
            XPYT_HOLDING_GIL(m_comms.erase(id))
        });
        m_comms[comm.comm_id()] = pycomm;
    }

    /***************
     * comm module *
     ***************/

    py::module get_comm_module_impl()
    {
        py::module comm_module = create_module("comm");

        py::class_<xcomm>(comm_module, "Comm")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&, py::kwargs>(),
                "target_name"_a="", "data"_a=py::dict(), "metadata"_a=py::dict(), "buffers"_a=py::list()
            )
            .def("close", &xcomm::close, "data"_a=py::dict(), "metadata"_a=py::dict(), "buffers"_a=py::list())
            .def("send", &xcomm::send, "data"_a=py::dict(), "metadata"_a=py::dict(), "buffers"_a=py::list())
            .def("on_msg", &xcomm::on_msg)
            .def("on_close", &xcomm::on_close)
            .def_property_readonly("comm_id", &xcomm::comm_id)
            .def_property_readonly("kernel", &xcomm::kernel);

        py::class_<xcomm_manager>(comm_module, "CommManager")
            .def(py::init<>())
            .def("register_target", &xcomm_manager::register_target)
            .def("register_comm", &xcomm_manager::register_comm);

        comm_module.def("get_comm_manager", [&comm_module]() {
            static py::object comm_manager = comm_module.attr("CommManager")();
            return comm_manager;
        });

        comm_module.def("create_comm", [&comm_module](py::args objs, py::kwargs kw) {
            py::object comm = comm_module.attr("Comm")(*objs, **kw);
            comm_module.attr("get_comm_manager")().attr("register_comm")(comm);
            return comm;
        });

        return comm_module;
    }

    py::module get_comm_module()
    {
        static py::module comm_module = get_comm_module_impl();
        return comm_module;
    }
}
