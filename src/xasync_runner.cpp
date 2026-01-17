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
        std::cout<< "xasync_runner created" << std::endl;
    }
    
    void xasync_runner::run_impl()
    {
        std::cout << "get descriptors "<< std::endl;
        //#ifndef _WIN32
            int fd_shell_int = static_cast<int>(this->get_shell_fd());
            int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());
        // #else
        //     //  just use placeholders on Windows
        //     int fd_shell_int = 0;
        //     int fd_controller_int = 0;
        // #endif

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


        auto func = py::cpp_function([](
            std::string msg
        ) {
            std::cout << "Received message in Python: " << msg << std::endl;
        });

        exec(R"(
        
        import sys
        is_win = sys.platform.startswith("win") or sys.platform.startswith("cygwin") or sys.platform.startswith("msys")
        print("is_win:", is_win)

        sys.stdout.write(f"is win: {is_win}\n")
        sys.stdout.flush()
        import asyncio
        if is_win:

            async def loop_shell(fd_shell, shell_callback, func):
                while True:
                    await asyncio.sleep()
                    func("polling for shell message")
                    shell_callback()
                    func("done polling for shell message")
            async def loop_controller(fd_controller, controller_callback, func):
                while True:
                    await asyncio.sleep()
                    func("polling for controller message")
                    controller_callback()                
                    func("done polling for controller message")

            def run_main_impl(fd_shell, fd_controller, shell_callback, controller_callback, func):
                func("Starting async loop on Windows")
                # here we create / ensure we have an event loop
                try:
                    loop = asyncio.get_event_loop()
                except Exception as e:
                    func(f"Exception getting event loop: {e}")
                loop = asyncio.get_event_loop()

                func("Creating tasks for shell and controller")
                task_shell = loop.create_task(loop_shell(fd_shell, shell_callback, func))

                func("Creating task for controller")
                task_controller = loop.create_task(loop_controller(fd_controller, controller_callback, func))
                
                func("Running event loop forever")
                loop.run_forever()
        else:
            def run_main_impl(fd_shell, fd_controller, shell_callback, controller_callback, func):

                # here we create / ensure we have an event loop
                loop = asyncio.get_event_loop()

                loop.add_reader(fd_shell, shell_callback)
                loop.add_reader(fd_controller, controller_callback)

                loop.run_forever()
        
        def run_main(fd_shell, fd_controller, shell_callback, controller_callback, func):
            try:
                run_main_impl(fd_shell, fd_controller, shell_callback, controller_callback, func)
            except Exception as e:
                func(f"Exception in run_main: {e}"

        )", m_global_dict);

        py::object run_func = m_global_dict["run_main"];
        std::cout << "Starting async loop "<< std::endl;
        run_func(fd_shell_int, fd_controller_int, shell_callback, controller_callback, func);
        
    
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

                )", m_global_dict);

                py::object stop_func = m_global_dict["stop_loop"];
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
