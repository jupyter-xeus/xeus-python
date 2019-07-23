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
#include <memory>
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

        using gil_scoped_release_ptr = std::unique_ptr<py::gil_scoped_release>;

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

        nl::json internal_request_impl(const nl::json& content) override;

        void redirect_output();
        void redirect_display();

        py::object m_displayhook;
        std::vector<std::string> m_inputs;

        // The interpreter has the same scope as a `gil_scoped_release` instance
        // so that the GIL is not held by default, it will only be held when the
        // interpreter wants to execute Python code. This means that whenever
        // the interpreter will execute Python code it will need to create an
        // `gil_scoped_acquire` instance first.
        gil_scoped_release_ptr m_release_gil = nullptr;
    };
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif
