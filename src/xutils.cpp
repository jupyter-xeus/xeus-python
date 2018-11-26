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
#include <stdexcept>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"

#include "xutils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{

    zmq::message_t pybytes_to_zmq_message(py::bytes bytes)
    {
        char* buffer;
        Py_ssize_t length;
#if PY_MAJOR_VERSION >= 3
        PyBytes_AsStringAndSize(bytes.ptr(), &buffer, &length);
#else
        PyString_AsStringAndSize(bytes.ptr(), &buffer, &length);
#endif
        return zmq::message_t(buffer, length);
    }

    nl::json pyobj_to_nljson(py::object obj)
    {
        py::module py_json = py::module::import("json");

        return nl::json::parse(static_cast<std::string>(
            py::str(py_json.attr("dumps")(obj))
        ));
    }

    py::object nljson_to_pyobj(const nl::json& json)
    {
        py::module py_json = py::module::import("json");

        return py_json.attr("loads")(json.dump());
    }

    py::list zmq_buffers_to_pylist(const std::vector<zmq::message_t>& buffers)
    {
        py::list bufferlist;
        for (const zmq::message_t& buffer: buffers)
        {
            const char* buf = buffer.data<const char>();
            bufferlist.attr("append")(py::bytes(buf));
        }
        return bufferlist;
    }

    std::vector<zmq::message_t> pylist_to_zmq_buffers(py::list bufferlist)
    {
        std::vector<zmq::message_t> buffers;
        for (py::handle buffer: bufferlist)
        {
            if (py::isinstance<py::memoryview>(buffer))
            {
                py::bytes bytes = buffer.attr("tobytes")();
                buffers.push_back(pybytes_to_zmq_message(bytes));
            }
            else
            {
                buffers.push_back(pybytes_to_zmq_message(buffer.cast<py::bytes>()));
            }
        }
        return buffers;
    }

    py::object cppmessage_to_pymessage(const xeus::xmessage& msg)
    {
        py::dict py_msg = (
            "header"_a, nljson_to_pyobj(msg.header()),
            "parent_header"_a, nljson_to_pyobj(msg.parent_header()),
            "metadata"_a, nljson_to_pyobj(msg.metadata()),
            "content"_a, nljson_to_pyobj(msg.content()),
            "buffers"_a, zmq_buffers_to_pylist(msg.buffers())
        );

        return py_msg;
    }

}
