/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/eval.h"

#include "xutils.hpp"

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
#if PY_MAJOR_VERSION >= 3
        PyBytes_AsStringAndSize(bytes.ptr(), &buffer, &length);
#else
        PyString_AsStringAndSize(bytes.ptr(), &buffer, &length);
#endif
        return zmq::message_t(buffer, static_cast<std::size_t>(length));
    }

    py::list zmq_buffers_to_pylist(const std::vector<zmq::message_t>& buffers)
    {
        py::list bufferlist;
        for (const zmq::message_t& buffer : buffers)
        {
            const char* buf = buffer.data<const char>();
            bufferlist.attr("append")(py::bytes(buf));
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

#if PY_MAJOR_VERSION == 2
    bool holding_gil()
    {
        PyThreadState * tstate = _PyThreadState_Current;
        return tstate && (tstate == PyGILState_GetThisThreadState());
    }
#else
    bool holding_gil()
    {
        return PyGILState_Check();
    }
#endif

    void exec(const py::object& code, const py::object& scope)
    {
        // Workaround for https://github.com/pybind/pybind11/issues/1654
        if (scope.attr("get")("__builtins__").is_none())
        {
            scope["__builtins__"] = py::module::import(XPYT_BUILTINS);
        }
        py::exec(XPYT_EXEC_COMMAND, py::globals(), py::dict(py::arg("_code_") = code, py::arg("_scope_") = scope));
    }

    std::string get_tmp_prefix()
    {
        static std::string tmp_prefix = xeus::get_temp_directory_path()
                                      + "/xpython_"
                                      + std::to_string(xeus::get_current_pid());
        return tmp_prefix;
    }
}

namespace nlohmann
{
    namespace detail
    {
        py::object from_json_impl(const json& j)
        {
            if (j.is_null())
            {
                return py::none();
            }
            else if (j.is_boolean())
            {
                return py::bool_(j.get<bool>());
            }
            else if (j.is_number())
            {
                double number = j.get<double>();
                if (number == std::floor(number))
                {
                    return py::int_(j.get<long>());
                }
                else
                {
                    return py::float_(number);
                }
            }
            else if (j.is_string())
            {
                return py::str(j.get<std::string>());
            }
            else if (j.is_array())
            {
                py::list obj;
                for (const auto& el : j)
                {
                    obj.attr("append")(from_json_impl(el));
                }
                return obj;
            }
            else // Object
            {
                py::dict obj;
                for (json::const_iterator it = j.cbegin(); it != j.cend(); ++it)
                {
                    obj[py::str(it.key())] = from_json_impl(it.value());
                }
                return obj;
            }
        }

        json to_json_impl(const py::handle& obj)
        {
            if (obj.is_none())
            {
                return nullptr;
            }
            if (py::isinstance<py::bool_>(obj))
            {
                return obj.cast<bool>();
            }
            if (py::isinstance<py::int_>(obj))
            {
                return obj.cast<long>();
            }
            if (py::isinstance<py::float_>(obj))
            {
                return obj.cast<double>();
            }
            if (py::isinstance<py::str>(obj))
            {
                return obj.cast<std::string>();
            }
            if (py::isinstance<py::tuple>(obj) || py::isinstance<py::list>(obj))
            {
                auto out = json::array();
                for (const py::handle& value : obj)
                {
                    out.push_back(to_json_impl(value));
                }
                return out;
            }
            if (py::isinstance<py::dict>(obj))
            {
                auto out = json::object();
                for (const py::handle& key : obj)
                {
                    out[py::str(key).cast<std::string>()] = to_json_impl(obj[key]);
                }
                return out;
            }
            throw std::runtime_error("to_json not implemented for this type of object: " + obj.cast<std::string>());
        }
    }

    py::object adl_serializer<py::object>::from_json(const json& j)
    {
        return detail::from_json_impl(j);
    }

    void adl_serializer<py::object>::to_json(json& j, const py::object& obj)
    {
        j = detail::to_json_impl(obj);
    }
}
