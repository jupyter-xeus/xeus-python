/***************************************************************************
* Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_PYTHON_LOGGER_HPP
#define XPYT_PYTHON_LOGGER_HPP

#include <string>
#include <vector>

namespace xpyt
{
    class xpython_logger
    {

    public:

        using logger_function_type = std::function<void(const std::string&)>;
        using loggers_type = std::vector<logger_function_type>;

        xpython_logger();
        virtual ~xpython_logger();

        void add_logger(logger_function_type logger);
        void write(const std::string& message);

    private:

        loggers_type m_loggers;
    };
}

#endif
