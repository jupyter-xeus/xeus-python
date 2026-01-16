/***************************************************************************
* Copyright (c) 2024, Isabel Paredes                                       *
* Copyright (c) 2024, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XEUS_PYTHON_ASYNC_RUNNER_HPP
#define XEUS_PYTHON_ASYNC_RUNNER_HPP

#include <memory>

#include "xeus-python/xeus_python_config.hpp"
#include "xeus-zmq/xshell_runner.hpp"
#include "pybind11/pybind11.h"


namespace py = pybind11;

namespace xpyt
{

    class XEUS_PYTHON_API xasync_runner final : public xeus::xshell_runner
    {
    public:

        xasync_runner(py::dict globals);

        xasync_runner(const xasync_runner&) = delete;
        xasync_runner& operator=(const xasync_runner&) = delete;
        xasync_runner(xasync_runner&&) = delete;
        xasync_runner& operator=(xasync_runner&&) = delete;

        ~xasync_runner() override = default;
        
    private:
        void on_message_doorbell_shell();
        void on_message_doorbell_controller();

        void run_impl() override;
        
        py::dict m_global_dict;

    };






} // namespace xeus

#endif // XEUS_PYTHON_ASYNC_RUNNER_HPP
