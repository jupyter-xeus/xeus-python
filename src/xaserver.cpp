
#include "xeus-python/xaserver.hpp"
#include "xasync_runner.hpp"
#include "xeus-python/xacontrol_default_runner.hpp"
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
                std::make_unique<xpyt::xacontrol_default_runner>(),
                std::make_unique<xpyt::xasync_runner>(globals)
            );
        };
    }


} // namespace xeus
