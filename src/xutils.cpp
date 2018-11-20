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

#include "xeus/xjson.hpp"
#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"

#include "xutils.hpp"

namespace py = pybind11;

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

    xeus::xjson pydict_to_xjson(py::dict dict)
    {
        py::module py_json = py::module::import("json");

        return xeus::xjson::parse(static_cast<std::string>(
            py::str(py_json.attr("dumps")(dict))
        ));
    }

    py::dict xjson_to_pydict(const xeus::xjson& json)
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
        for (py::handle bytes: bufferlist)
        {
            buffers.push_back(pybytes_to_zmq_message(bytes.cast<py::bytes>()));
        }
        return buffers;
    }

}
