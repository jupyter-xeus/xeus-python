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
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/eval.h"

#include "xinternal_utils.hpp"

#ifdef WIN32
#include "Windows.h"
#endif

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    std::string red_text(const std::string& text)
    {
        return "\033[0;31m" + text + "\033[0m";
    }

    std::string green_text(const std::string& text)
    {
        return "\033[0;32m" + text + "\033[0m";
    }

    std::string blue_text(const std::string& text)
    {
        return "\033[0;34m" + text + "\033[0m";
    }

    zmq::message_t pybytes_to_zmq_message(py::bytes bytes)
    {
        char* buffer;
        Py_ssize_t length;
        PyBytes_AsStringAndSize(bytes.ptr(), &buffer, &length);
        return zmq::message_t(buffer, static_cast<std::size_t>(length));
    }

    py::list zmq_buffers_to_pylist(const std::vector<zmq::message_t>& buffers)
    {
        py::list bufferlist;
        for (const zmq::message_t& buffer : buffers)
        {
            const char* buf = buffer.data<const char>();
            bufferlist.attr("append")(py::memoryview(py::bytes(buf)));
        }
        return bufferlist;
    }

    std::vector<zmq::message_t> pylist_to_zmq_buffers(const py::object& bufferlist)
    {
        std::vector<zmq::message_t> buffers;

        // Cannot iterate over NoneType, returning immediately with an empty vector
        if (bufferlist.is_none())
        {
            return buffers;
        }

        for (py::handle buffer : bufferlist)
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
        py::dict py_msg;
        py_msg["header"] = msg.header().get<py::object>();
        py_msg["parent_header"] = msg.parent_header().get<py::object>();
        py_msg["metadata"] = msg.metadata().get<py::object>();
        py_msg["content"] = msg.content().get<py::object>();
        py_msg["buffers"] = zmq_buffers_to_pylist(msg.buffers());

        return py_msg;
    }
}
