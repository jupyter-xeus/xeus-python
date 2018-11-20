/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_COMM_HPP
#define XPYT_COMM_HPP

#include <string>
#include <functional>

#include "xeus/xcomm.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

namespace py = pybind11;

namespace xpyt
{
    class xcomm
    {
    public:

        using python_callback_type = std::function<void(py::object)>;
        using cpp_callback_type = std::function<void(const xeus::xmessage&)>;
        using zmq_buffers_type = std::vector<zmq::message_t>;

        xcomm(py::args args, py::kwargs kwargs);
        virtual ~xcomm();

        std::string comm_id() const;
        bool kernel() const;

        void close(py::args args, py::kwargs kwargs);
        void send(py::args args, py::kwargs kwargs);
        void on_msg(python_callback_type callback);
        void on_close(python_callback_type callback);

    private:

        xeus::xtarget* target(py::kwargs kwargs) const;
        xeus::xguid id(py::kwargs kwargs) const;
        xeus::xjson json_data(py::kwargs kwargs) const;
        xeus::xjson json_metadata(py::kwargs kwargs) const;
        zmq_buffers_type zmq_buffers(py::kwargs kwargs) const;
        py::dict py_message(const xeus::xmessage& message) const;
        cpp_callback_type cpp_callback(python_callback_type callback) const;

        xeus::xcomm m_comm;
    };
}

#endif
