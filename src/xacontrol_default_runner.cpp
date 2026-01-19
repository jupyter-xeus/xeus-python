/***************************************************************************
* Copyright (c) 2016, Johan Mabille, Sylvain Corlay, Martin Renou          *
* Copyright (c) 2016, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "xeus-python/xacontrol_default_runner.hpp"

#include <iostream>

namespace xpyt
{
    void xacontrol_default_runner::run_impl() 
    {
        std::clog << "Starting control default runner..." << std::endl;
        m_request_stop = false;

        while (!m_request_stop)
        {
            auto msg = read_control();
            if (msg.has_value())
            {
                std::cout << "Control default runner received a message." << msg.value().header().dump(4)<< std::endl;
                notify_control_listener(std::move(msg.value()));
                std::cout << "Notified control listener." << std::endl;
            }
        }

        stop_channels();
    }

    void xacontrol_default_runner::stop_impl()
    {
        std::clog << "Stopping control default runner..." << std::endl;
        m_request_stop = true;
    }
}

