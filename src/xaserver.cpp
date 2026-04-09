/***************************************************************************
* Copyright (c) 2026, Thorsten Beier                                       *
* Copyright (c) 2024, Isabel Paredes                                       *
* Copyright (c) 2024, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/
#include "xeus-python/xaserver.hpp"
#include "xasync_runner.hpp"
#include "xeus-zmq/xcontrol_default_runner.hpp"
#include "xeus-zmq/xserver_zmq_split.hpp"

#include "xeus/xkernel.hpp"
#include "xeus/xserver.hpp"
#include "xeus/xeus_context.hpp"
#include "xeus/xkernel_configuration.hpp"

#include "nlohmann/json.hpp"


namespace nl = nlohmann;

namespace  xpyt
{
    xeus::xkernel::server_builder make_xaserver_factory(py::dict globals)
    {
        return [globals](
            xeus::xcontext& context,
            const xeus::xconfiguration& config,
             nl::json::error_handler_t eh)
        {
            return xeus::make_xserver_shell
            (
                context,
                config,
                eh,
                std::make_unique<xeus::xcontrol_default_runner>(),
                std::make_unique<xpyt::xasync_runner>(globals)
            );
        };
    }


} // namespace xpyt
