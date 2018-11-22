/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_LOGGER_HPP
#define XPYT_LOGGER_HPP

#include <string>

namespace xpyt
{
    class xlogger
    {
    public:

        xlogger(std::string pipe_name);
        virtual ~xlogger();

        void write(const std::string& message);

    private:

        std::string m_pipe_name;
    };
}

#endif
