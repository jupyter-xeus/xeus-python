#include <thread>

#include "xdebugger.hpp"

namespace xpyt
{
    debugger::debugger(zmq::context_t& context,
                       const xeus::xconfiguration&)
    {
    }

    nl::json debugger::process_request_impl(const nl::json& message)
    {
        return message;
    }
    
    std::unique_ptr<xeus::xdebugger> make_python_debugger(zmq::context_t& context,
                                                          const xeus::xconfiguration& config)
    {
        return std::unique_ptr<xeus::xdebugger>(new debugger(context, config));
    }
}

