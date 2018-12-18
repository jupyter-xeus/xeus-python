/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/


#ifndef XPYT_UTILS_HPP
#define XPYT_UTILS_HPP

#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"

namespace py = pybind11;
namespace nl = nlohmann;

#if PY_MAJOR_VERSION == 2
    #define XPYT_BUILTINS "__builtin__"
#else
    #define XPYT_BUILTINS "builtins"
#endif

namespace xpyt
{
    std::string red_text(const std::string& text);

    py::list zmq_buffers_to_pylist(const std::vector<zmq::message_t>& buffers);
    std::vector<zmq::message_t> pylist_to_zmq_buffers(const py::list& bufferlist);

    py::object cppmessage_to_pymessage(const xeus::xmessage& msg);
}

namespace nlohmann
{
    template <>
    struct adl_serializer<py::object>
    {
        static py::object from_json(const json& j);
        static void to_json(json& j, const py::object& obj);
    };
}

#endif
