/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_INTERPRETER_HPP
#define XPYT_INTERPRETER_HPP

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11/pybind11.h"

#include "xeus_python_config.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    class XEUS_PYTHON_API interpreter : public xeus::xinterpreter
    {
    public:

        interpreter(int argc, const char* const* argv);
        virtual ~interpreter();

    protected:

        void configure_impl() override;

        nl::json execute_request_impl(int execution_counter,
                                      const std::string& code,
                                      bool silent,
                                      bool store_history,
                                      nl::json user_expressions,
                                      bool allow_stdin) override;

        nl::json complete_request_impl(const std::string& code, int cursor_pos) override;

        nl::json inspect_request_impl(const std::string& code,
                                      int cursor_pos,
                                      int detail_level) override;

        nl::json is_complete_request_impl(const std::string& code) override;

        nl::json kernel_info_request_impl() override;

        void shutdown_request_impl() override;

        void redirect_output();
        void redirect_display();

        py::object m_displayhook;
        std::vector<std::string> m_inputs;
    };
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif
