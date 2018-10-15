/***************************************************************************
* Copyright (c) 2016, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <string>

#include "Python.h"

#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"

#include "xpython_interpreter.hpp"

std::string extract_filename(int& argc, char* argv[])
{
    std::string res = "";
    for (int i = 0; i < argc; ++i)
    {
        if ((std::string(argv[i]) == "-f") && (i + 1 < argc))
        {
            res = argv[i + 1];
            for(int j = i; j < argc - 2; ++j)
            {
                argv[j]  = argv[j + 2];
            }
            argc -= 2;
            break;
        }
    }
    return res;
}


int main(int argc, char* argv[])
{
    using interpreter_ptr = std::unique_ptr<xpyt::python_interpreter>;

    std::string file_name = (argc == 1) ? "connection.json" : extract_filename(argc, argv);
    xeus::xconfiguration config = xeus::load_configuration(file_name);

    interpreter_ptr interpreter = interpreter_ptr(new xpyt::python_interpreter(argc, argv));
    xeus::xkernel kernel(config, xeus::get_user_name(), std::move(interpreter));
    std::cout << "starting xeus-python kernel" << std::endl;
    kernel.start();

    return 0;
}
