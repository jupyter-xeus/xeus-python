/***************************************************************************
* Copyright (c) 2016, Johan Mabille, Sylvain Corlay, Martin Renou          *
* Copyright (c) 2016, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XEUS_PYTHON_ACONTROL_DEFAULT_RUNNER_HPP
#define XEUS_PYTHON_ACONTROL_DEFAULT_RUNNER_HPP

#include "xeus_python_config.hpp"
#include "xeus-zmq/xcontrol_runner.hpp"

namespace xpyt
{
    class XEUS_PYTHON_API xacontrol_default_runner final : public xeus::xcontrol_runner
    {
    public:

        xacontrol_default_runner() = default;
        ~xacontrol_default_runner() override = default;

    private:

        void run_impl() override;
        void stop_impl() override;
        
        bool m_request_stop;
    };
}

#endif

