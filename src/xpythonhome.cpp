/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "xeus-python/xpythonhome.hpp"

namespace xpyt
{
    // This functino must not be inlined in the header.
    // get_pythonhome and set_pythonhome must remain
    // in different compilation units to avoid compiler
    // optimization in set_pythonhome that results
    // in a relocation issue with conda.
    const char* get_pythonhome()
    {
#ifdef XEUS_PYTHONHOME
        return XEUS_PYTHONHOME;
#else
        return "";
#endif
    }
}

