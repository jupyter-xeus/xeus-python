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

namespace xpyt
{
    /*******************
     * executable_path *
     *******************/

    std::string executable_path();

    /***************
     * prefix_path *
     ***************/

    std::string prefix_path();

    /**************************
     * python home and prefix *
     **************************/

    std::string get_python_prefix();
    std::string get_python_path();
    void set_pythonhome();
}
#endif
