/***************************************************************************
* Copyright (c) 2026, Thorsten Beier                                       *
* Copyright (c) 2024, Isabel Paredes                                       *
* Copyright (c) 2024, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <memory>

#include "xasync_runner.hpp"
#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{

    xasync_runner::xasync_runner(py::dict globals)
        : xeus::xshell_runner(),
          m_global_dict{globals}
    {
    }
    
    void xasync_runner::run_impl()
    {

        const int fd_shell_int = static_cast<int>(this->get_shell_fd());
        const int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());

        // wrap c++ callbacks into python functions
        py::cpp_function shell_callback = py::cpp_function([this]() {
            this->on_message_doorbell_shell();
        });
        py::cpp_function controller_callback = py::cpp_function([this]() {
            this->on_message_doorbell_controller();
        });

        // ensure gil
        py::gil_scoped_acquire acquire;

        // pure python impl of the main loop
        exec(R"(
        import sys
        import asyncio
        import traceback
        import socket

        is_win = sys.platform.startswith("win") or sys.platform.startswith("cygwin") or sys.platform.startswith("msys")

        if is_win:
            asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

        class ZMQSockReader:
            def __init__(self, fd):
                self._fd = fd
                self._sock = socket.fromfd(self._fd, socket.AF_INET, socket.SOCK_STREAM)

            def fileno(self):
                return self._fd

            def __del__(self):
                try:
                    self._sock.detach()
                except:
                    pass
                
        def make_fd(fd):
            if is_win:
                return ZMQSockReader(fd)
            else:
                return fd

        def run_main_non_busy_loop(fd_shell, fd_controller, shell_callback, controller_callback):
            # here we create / ensure we have an event loop
            loop = asyncio.new_event_loop() 
            asyncio.set_event_loop(loop)
            loop.add_reader(fd_shell, shell_callback)
            loop.add_reader(fd_controller, controller_callback)
            loop.run_forever()
    
        def run_main(fd_shell, fd_controller, shell_callback, controller_callback):
            try:
                fd_controller = make_fd(fd_controller)
                fd_shell = make_fd(fd_shell)
                run_main_non_busy_loop(fd_shell, fd_controller, shell_callback, controller_callback)

            except Exception as e:
                traceback_str = traceback.format_exc()
                print(f"Exception in async runner: {e}\n{traceback_str}", file=sys.stderr)
                sys.exit(1)

        )", m_global_dict);

        m_global_dict["run_main"](fd_shell_int, fd_controller_int, shell_callback, controller_callback);
    
    }

    void xasync_runner::on_message_doorbell_shell()
    {
        int ZMQ_DONTWAIT{ 1 }; // from zmq.h 
        while (auto msg = read_shell(ZMQ_DONTWAIT))
        {
            notify_shell_listener(std::move(msg.value()));
        }
    }

    void xasync_runner::on_message_doorbell_controller()
    {
        int ZMQ_DONTWAIT{ 1 }; // from zmq.h
        while (auto msg = read_controller(ZMQ_DONTWAIT))
        {
            std::string val{ msg.value() };

            if (val == "stop")
            {
                send_controller(std::move(val));

                int fd_shell_int = static_cast<int>(this->get_shell_fd());
                int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());

                py::gil_scoped_acquire acquire;

                 // Or create via exec if you need a more complex function:
                py::exec(R"(
                import asyncio
                import sys
                
                def stop_loop(fd_shell, fd_controller, use_busy_loop):
                    loop = asyncio.get_event_loop()
                    if not use_busy_loop:
                        loop.remove_reader(make_fd(fd_shell))
                        loop.remove_reader(make_fd(fd_controller))
                    loop.stop()

                )", m_global_dict);

                py::object stop_func = m_global_dict["stop_loop"];
                stop_func(fd_shell_int, fd_controller_int, m_use_busy_loop);
                break;

            }
            else
            {
                std::string rep = notify_internal_listener(std::move(val));
                send_controller(std::move(rep));
            }
        }
    }

} // namespace xpyt
