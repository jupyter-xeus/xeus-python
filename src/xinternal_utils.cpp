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
    py::module create_module(const std::string& module_name)
    {
        return py::module_::create_extension_module(module_name.c_str(), nullptr, new py::module_::module_def);
    }

    xeus::binary_buffer pybytes_to_cpp_message(py::bytes bytes)
    {
        char* buffer;
        Py_ssize_t length;
        PyBytes_AsStringAndSize(bytes.ptr(), &buffer, &length);
        return xeus::binary_buffer(buffer, buffer + static_cast<std::size_t>(length));
    }

    py::list cpp_buffers_to_pylist(const xeus::buffer_sequence& buffers)
    {
        py::list bufferlist;
        for (const xeus::binary_buffer& buffer : buffers)
        {
            const char* buf = buffer.data();
            bufferlist.attr("append")(py::memoryview(py::bytes(buf)));
        }
        return bufferlist;
    }

    xeus::buffer_sequence pylist_to_cpp_buffers(const py::object& bufferlist)
    {
        xeus::buffer_sequence buffers;

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
                buffers.push_back(pybytes_to_cpp_message(bytes));
            }
            else
            {
                buffers.push_back(pybytes_to_cpp_message(buffer.cast<py::bytes>()));
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
        py_msg["buffers"] = cpp_buffers_to_pylist(msg.buffers());

        return py_msg;
    }

    std::string get_tmp_prefix()
    {
        return xeus::get_tmp_prefix("xpython");
    }

    std::string get_tmp_suffix()
    {
        return ".py";
    }

    std::string get_cell_tmp_file(const std::string& content)
    {
        return xeus::get_cell_tmp_file(get_tmp_prefix(),
                                       content,
                                       get_tmp_suffix());
    }
}
