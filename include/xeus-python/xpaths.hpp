/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_PATHS_HPP
#define XPYT_PATHS_HPP

#include <string>

#include "xeus_python_config.hpp"

namespace xpyt
{
    /**************************
     * python home and prefix *
     **************************/

    XEUS_PYTHON_API std::string get_python_prefix();
    XEUS_PYTHON_API std::string get_python_path();
    XEUS_PYTHON_API void set_pythonhome();
}

#endif
