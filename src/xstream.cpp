/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <string>
#include <sstream>

#include "xeus/xinterpreter.hpp"

#include "pybind11/functional.h"
#include "pybind11/pybind11.h"

#include "xstream.hpp"
#include "xinternal_utils.hpp"

namespace py = pybind11;

namespace xpyt
{

    /***********************
     * xstream declaration *
     ***********************/

    class xstream
    {
    public:

        xstream(std::string stream_name);
        virtual ~xstream();

        void write(const std::string& message);
        void flush();
        bool isatty();

    private:

        std::string m_stream_name;
    };

    /**************************
     * xstream implementation *
     **************************/

    xstream::xstream(std::string stream_name)
        : m_stream_name(stream_name)
    {
    }

    xstream::~xstream()
    {
    }

    void xstream::write(const std::string& message)
    {
        xeus::get_interpreter().publish_stream(m_stream_name, message);
    }

    void xstream::flush()
    {
    }

    bool xstream::isatty()
    {
        return false;
    }

    /********************************
     * xterminal_stream declaration *
     ********************************/

    class xterminal_stream
    {
    public:

        xterminal_stream();
        virtual ~xterminal_stream();

        void write(const std::string& message);
        void flush();
    };

    /***********************************
     * xterminal_stream implementation *
     ***********************************/

    xterminal_stream::xterminal_stream()
    {
    }

    xterminal_stream::~xterminal_stream()
    {
    }

    void xterminal_stream::write(const std::string& message)
    {
        std::cout << message;
    }

    void xterminal_stream::flush()
    {
    }

    /*****************
     * stream module *
     *****************/

    py::module get_stream_module_impl()
    {
        py::module stream_module = create_module("stream");

        py::class_<xstream>(stream_module, "Stream")
            .def(py::init<std::string>())
            .def("write", &xstream::write)
            .def("flush", &xstream::flush)
            .def("isatty", &xstream::isatty);

        py::class_<xterminal_stream>(stream_module, "TerminalStream")
            .def(py::init<>())
            .def("write", &xterminal_stream::write)
            .def("flush", &xterminal_stream::flush);

        return stream_module;
    }

    py::module get_stream_module()
    {
        static py::module stream_module = get_stream_module_impl();
        return stream_module;
    }
}
