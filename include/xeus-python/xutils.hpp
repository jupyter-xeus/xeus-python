/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
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


namespace xpyt
{
    std::string red_text(const std::string& text);
    std::string green_text(const std::string& text);
    std::string blue_text(const std::string& text);

    py::list zmq_buffers_to_pylist(const std::vector<zmq::message_t>& buffers);
    std::vector<zmq::message_t> pylist_to_zmq_buffers(const py::object& bufferlist);

    py::object cppmessage_to_pymessage(const xeus::xmessage& msg);

    bool is_pyobject_true(const py::object& obj);

    bool holding_gil();

#define XPYT_HOLDING_GIL(func)           \
    if (holding_gil())                   \
    {                                    \
        func;                            \
    }                                    \
    else                                 \
    {                                    \
        py::gil_scoped_acquire acquire;  \
        func;                            \
    }

    void exec(const py::object& code, const py::object& scope = py::globals());
    py::object eval(const py::object& code, const py::object& scope = py::globals());

    size_t get_hash_seed();
    std::string get_tmp_prefix();
    std::string get_tmp_suffix();
    std::string get_cell_tmp_file(const std::string& content);
}

#endif
