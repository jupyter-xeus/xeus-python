/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_COMM_HPP
#define XPYT_COMM_HPP

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    class xcomm
    {
    public:

        using close_callback_type = std::function<void()>;
        using python_callback_type = std::function<void(py::object)>;
        using cpp_callback_type = std::function<void(const xeus::xmessage&)>;
        using buffers_sequence = xeus::buffer_sequence;

        xcomm(const py::object& target_name, const py::object& data, const py::object& metadata, const py::object& buffers, const py::kwargs& kwargs);
        xcomm(xeus::xcomm&& comm);
        xcomm(xcomm&& comm) = delete;
        xcomm& operator=(xcomm&& rhs) = delete;
        xcomm(const xcomm&) = delete;
        xcomm& operator=(xcomm& rhs) = delete;
        ~xcomm() = default;

        std::string comm_id() const;
        bool kernel() const;

        void close(const py::object& data, const py::object& metadata, const py::object& buffers);
        void send(const py::object& data, const py::object& metadata, const py::object& buffers);
        void on_msg(const python_callback_type& callback);
        void on_close(const python_callback_type& callback);

        void on_close_cleanup(close_callback_type callback);

    private:

        // Warning: this function creates and register the target with a dummy
        // comm_open handler when the target does not exist, so that create_comm
        // has the same behavior as ipykernel.
        const xeus::xtarget* target(const py::object& target_name) const;
        xeus::xguid id(const py::kwargs& kwargs) const;
        cpp_callback_type cpp_callback(const python_callback_type& callback) const;
        cpp_callback_type cpp_close_callback(const python_callback_type& callback) const;

        xeus::xcomm m_comm;
        close_callback_type m_close_callback;
    };

    struct xcomm_manager
    {
        xcomm_manager() = default;

        void register_target(const py::str& target_name, const py::object& callback);
        void register_comm(py::object comm);
    };

    py::module get_comm_module();

}

#endif
