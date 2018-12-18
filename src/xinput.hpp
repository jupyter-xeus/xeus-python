/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_INPUT_HPP
#define XPYT_INPUT_HPP

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    /**
     * Input_redirection is a scope guard implementing the redirection of
     * input() and getpass() to the frontend through an input_request message.
     *
     * For Python 2, this also redirects raw_input().
     */
    class input_redirection
    {
    public:

        input_redirection(bool allow_stdin);
        ~input_redirection();

    private:

        py::object m_sys_input;
        py::object m_sys_getpass;
        py::object m_sys_raw_input;  // Only used for Python 2
    };
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif
