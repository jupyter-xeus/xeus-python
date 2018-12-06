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

#include "nlohmann/json.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    class xdisplayhook
    {
    public:

        xdisplayhook();
        virtual ~xdisplayhook();

        void set_execution_count(int execution_count);
        void operator()(const py::object& obj, bool raw) const;

    private:

        nl::json display_pub_data(const py::object& obj) const;

        int m_execution_count;
    };
}

#endif
