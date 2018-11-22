/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_STREAM_HPP
#define XPYT_STREAM_HPP

#include <string>

namespace xpyt
{
    class xstream
    {
    public:

        xstream(std::string stream_name);
        virtual ~xstream();

        void write(const std::string& message);

    private:

        std::string m_stream_name;
    };
}

#endif
