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

#include <functional>

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

namespace py = pybind11;

namespace xpyt
{
    class xdisplayhook
    {
    public:

        using hook_function_type = std::function<void(int, py::object)>;
        using hooks_type = std::vector<hook_function_type>;

        xdisplayhook();
        virtual ~xdisplayhook();

        void set_execution_count(int execution_count);
        void add_hook(hook_function_type hook);
        void operator()(py::object obj);

    private:

        int m_execution_count;
        hooks_type m_hooks;
    };

    xeus::xjson display_pub_data(py::object obj);
}

#endif
