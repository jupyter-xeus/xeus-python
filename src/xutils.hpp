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

#include "xeus/xjson.hpp"
#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{

    xeus::xjson pydict_to_xjson(py::dict dict);
    py::dict xjson_to_pydict(const xeus::xjson& json);

    py::list zmq_buffers_to_pylist(const std::vector<zmq::message_t>& buffers);
    std::vector<zmq::message_t> pylist_to_zmq_buffers(py::list bufferlist);

}

#endif
