/***************************************************************************
* Copyright (c) 2024, Isabel Paredes                                       *
* Copyright (c) 2024, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>
#include <memory>

#include "xeus-python/xasync_runner.hpp"
#include "pybind11/embed.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{

    xasync_runner::xasync_runner()
        : xeus::xshell_runner()
    {
        std::cout<< "xasync_runner created" << std::endl;
    }
    
    void xasync_runner::run_impl()
    {
        std::cout << "get descriptors "<< std::endl;
        #ifndef _WIN32
            int fd_shell_int = static_cast<int>(this->get_shell_fd());
            int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());
        #else
            //  just use placeholders on Windows
            int fd_shell_int = 0;
            int fd_controller_int = 0;
        #endif

        std::cout << "Got descriptors: " << fd_shell_int << ", " << fd_controller_int << std::endl;

        // wrap this->on_message_doorbell_shell and this->on_message_doorbell_controller
        // into a py::cpp_function
        py::cpp_function shell_callback = py::cpp_function([this]() {
            this->on_message_doorbell_shell();
        });
        py::cpp_function controller_callback = py::cpp_function([this]() {
            this->on_message_doorbell_controller();
        });


        // ensure gil
        py::gil_scoped_acquire acquire;

        py::exec(R"(
        
        import sys
        is_win = sys.platform.startswith("win") or sys.platform.startswith("cygwin") or sys.platform.startswith("msys")
        
        import asyncio
        if is_win:

            async def loop_shell(fd_shell, shell_callback):
                while True:
                    await asyncio.sleep(0.01)
                    shell_callback()
                    controller_callback()
            async def loop_controller(fd_controller, controller_callback):
                while True:
                    await asyncio.sleep(0.01)
                    controller_callback()
                    shell_callback()
                

            def run_main(fd_shell, fd_controller, shell_callback, controller_callback):

                # here we create / ensure we have an event loop
                loop = asyncio.get_event_loop()

                task_shell = loop.create_task(loop_shell(fd_shell, shell_callback))
                task_controller = loop.create_task(loop_controller(fd_controller, controller_callback))

                loop.run_forever()
        else:
            def run_main(fd_shell, fd_controller, shell_callback, controller_callback):

                # here we create / ensure we have an event loop
                loop = asyncio.get_event_loop()

                loop.add_reader(fd_shell, shell_callback)
                loop.add_reader(fd_controller, controller_callback)

                loop.run_forever()

        )", py::globals());

        py::object run_func = py::globals()["run_main"];
        run_func(fd_shell_int, fd_controller_int, shell_callback, controller_callback);
        
    
    }

    void xasync_runner::on_message_doorbell_shell()
    {
        int ZMQ_DONTWAIT{ 1 }; // from zmq.h 
        while (auto msg = read_shell(ZMQ_DONTWAIT))
        {
            std::cout << "got message on shell "<<std::endl;
            notify_shell_listener(std::move(msg.value()));
        }
    }

    void xasync_runner::on_message_doorbell_controller()
    {
        int ZMQ_DONTWAIT{ 1 }; // from zmq.h
        while (auto msg = read_controller(ZMQ_DONTWAIT))
        {
            std::string val{ msg.value() };

            std::cout << "Controller message: " << val << std::endl;
            if (val == "stop")
            {
                send_controller(std::move(val));



                std::cout << "get descriptors to stop"<< std::endl;
                #ifndef _WIN32
                    int fd_shell_int = static_cast<int>(this->get_shell_fd());
                    int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());
                #else
                    //  just use placeholders on Windows
                    int fd_shell_int = 0;
                    int fd_controller_int = 0;
                #endif

                py::gil_scoped_acquire acquire;

                 // Or create via exec if you need a more complex function:
                py::exec(R"(
                import asyncio
                import sys
                
                def stop_loop(fd_shell, fd_controller):
                    is_win = sys.platform.startswith("win") or sys.platform.startswith("cygwin") or sys.platform.startswith("msys")
                    

                    # here we create / ensure we have an event loop
                    loop = asyncio.get_event_loop()
                    if not is_win:
                        loop.remove_reader(fd_shell)
                        loop.remove_reader(fd_controller)

                    loop.stop()

                )", py::globals());

                py::object stop_func = py::globals()["stop_loop"];
                stop_func(fd_shell_int, fd_controller_int);

            }
            else
            {
                std::string rep = notify_internal_listener(std::move(val));
                send_controller(std::move(rep));
            }
        }
    }



} // namespace xpyt
