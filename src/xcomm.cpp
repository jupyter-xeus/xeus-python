/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>
#include <vector>

#include "xeus/xinterpreter.hpp"
#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"

#include "xutils.hpp"
#include "xcomm.hpp"

namespace py = pybind11;

namespace xpyt
{

    xcomm::xcomm(py::args /*args*/, py::kwargs kwargs)
        : m_comm(target(kwargs), id(kwargs))
    {
        m_comm.open(json_metadata(kwargs), json_data(kwargs), zmq_buffers(kwargs));
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

    void xcomm::close(py::args /*args*/, py::kwargs kwargs)
    {
        m_comm.close(json_metadata(kwargs), json_data(kwargs), zmq_buffers(kwargs));
    }

    void xcomm::send(py::args /*args*/, py::kwargs kwargs)
    {
        m_comm.send(json_metadata(kwargs), json_data(kwargs), zmq_buffers(kwargs));
    }

    void xcomm::on_msg(python_callback_type callback)
    {
        m_comm.on_message(cpp_callback(callback));
    }

    void xcomm::on_close(python_callback_type callback)
    {
        m_comm.on_close(cpp_callback(callback));
    }

    xeus::xtarget* xcomm::target(py::kwargs kwargs) const
    {
        std::string target_name = static_cast<std::string>(py::str(kwargs["target_name"]));
        return xeus::get_interpreter().comm_manager().target(target_name);
    }

    xeus::xguid xcomm::id(py::kwargs kwargs) const
    {
        if (py::hasattr(kwargs, "comm_id"))
        {
            return static_cast<std::string>(py::str(kwargs["comm_id"]));
        }
        else {
            return xeus::new_xguid();
        }
    }

    xeus::xjson xcomm::json_data(py::kwargs kwargs) const
    {
        return pydict_to_xjson(kwargs.attr("get")("data", py::dict()));
    }

    xeus::xjson xcomm::json_metadata(py::kwargs kwargs) const
    {
        return pydict_to_xjson(kwargs.attr("get")("metadata", py::dict()));
    }

    auto xcomm::zmq_buffers(py::kwargs kwargs) const -> zmq_buffers_type
    {
        return pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()));
    }

    py::dict xcomm::py_message(const xeus::xmessage& msg) const
    {
        xeus::xjson json_msg;
        json_msg["header"] = msg.header();
        json_msg["parent_header"] = msg.parent_header();
        json_msg["metadata"] = msg.metadata();
        json_msg["content"] = msg.content();

        py::dict py_msg = xjson_to_pydict(json_msg);
        py_msg["buffers"] = zmq_buffers_to_pylist(msg.buffers());

        return py_msg;
    }

    auto xcomm::cpp_callback(python_callback_type py_callback) const -> cpp_callback_type
    {
        return [this, py_callback] (const xeus::xmessage& msg) {
            py_callback(py_message(msg));
        };
    }

    PYBIND11_EMBEDDED_MODULE(xeus_python_comm, m) {
        py::class_<xcomm>(m, "XPythonComm")
            .def(py::init<py::args, py::kwargs>())
            .def("close", &xcomm::close)
            .def("send", &xcomm::send)
            .def("on_msg", &xcomm::on_msg)
            .def("on_close", &xcomm::on_close)
            .def_property_readonly("comm_id", &xcomm::comm_id)
            .def_property_readonly("kernel", &xcomm::kernel);
    }
}
