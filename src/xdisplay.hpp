/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_DISPLAY_HPP
#define XPYT_DISPLAY_HPP

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

namespace py = pybind11;

namespace xpyt
{
    class xdisplayhook
    {
    public:

        xdisplayhook();
        virtual ~xdisplayhook();

        void set_execution_count(int execution_count);
        void operator()(py::object obj);

    private:

        int m_execution_count;
    };

    xeus::xjson display_pub_data(py::object obj);
}

#endif
