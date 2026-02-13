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
          m_global_dict{globals},
            m_use_busy_loop{true}
    {
        std::cout<< "xasync_runner created" << std::endl;
    }
    
    void xasync_runner::run_impl()
    {
        std::cout << "get descriptors "<< std::endl;
            int fd_shell_int = static_cast<int>(this->get_shell_fd());
            int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());


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


        // the usual print function is not working for debugging since 
        // its replaced by the kernel's stdout handling, so we define
        // a custom function that writes to sys.stdout directly
        auto raw_print = py::cpp_function([](
            std::string msg
        ) {
            std::cout << "Received message in Python: " << msg << std::endl;
        });

        exec(R"(
        import sys
        import asyncio
        import traceback

        is_win = sys.platform.startswith("win") or sys.platform.startswith("cygwin") or sys.platform.startswith("msys")

        if is_win:
            asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())


        class ZMQSockReader:
            def __init__(self, fd):
                self._fd = fd

            def fileno(self):
                # This allows asyncio's select to see the handle
                return self._fd
        
        def make_fd(fd):
            if is_win:
                return ZMQSockReader(fd)
            else:
                return fd


        async def busy_loop(fd, callback, name, raw_print):
            try:
                raw_print(f"Starting busy loop {name} ")
                while True:
                    await asyncio.sleep(0)
                    callback()
            except Exception as e:
                raw_print(f"Exception in busy_loop {name} : {e}")

        def run_main_busy_loop(fd_shell, fd_controller, shell_callback, controller_callback, raw_print):
            raw_print("Creating event loop busy loop")
            loop = asyncio.new_event_loop() 
            assign_event_loop = asyncio.set_event_loop(loop)

            task_shell = loop.create_task(busy_loop(fd_shell, shell_callback, "shell", raw_print))
            task_controller = loop.create_task(busy_loop(fd_controller, controller_callback, "controller", raw_print))
            
            raw_print("Running event loop forever")
            loop.run_forever()
        

        def run_main_non_busy_loop(fd_shell, fd_controller, shell_callback, controller_callback, raw_print):
            raw_print("Creating event loop non-busy loop")
            # here we create / ensure we have an event loop
            loop = asyncio.new_event_loop() 
            assign_event_loop = asyncio.set_event_loop(loop)
            raw_print("Adding readers to event loop")
            loop.add_reader(fd_shell, shell_callback)
            loop.add_reader(fd_controller, controller_callback)
            raw_print("Running event loop forever") 
            loop.run_forever()
    
        def run_main(fd_shell, fd_controller, shell_callback, controller_callback, use_busy_loop, raw_print):
            try:
                fd_controller = make_fd(fd_controller)
                fd_shell = make_fd(fd_shell)
                
                main = run_main_busy_loop if use_busy_loop else run_main_non_busy_loop
                main(fd_shell, fd_controller, shell_callback, controller_callback, raw_print)

            except Exception as e:
                # get full traceback

                traceback_str = traceback.format_exc()
                raw_print(f"Exception in run_main: {traceback_str}")

        )", m_global_dict);

        py::object run_func = m_global_dict["run_main"];
        std::cout << "Starting async loop "<< std::endl;
        run_func(fd_shell_int, fd_controller_int, shell_callback, controller_callback, m_use_busy_loop, raw_print);
        
    
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
                int fd_shell_int = static_cast<int>(this->get_shell_fd());
                int fd_controller_int = static_cast<int>(this-> get_shell_controller_fd());


                py::gil_scoped_acquire acquire;

                 // Or create via exec if you need a more complex function:
                py::exec(R"(
                import asyncio
                import sys
                
                def stop_loop(fd_shell, fd_controller, use_busy_loop):
                    

                    # here we create / ensure we have an event loop
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
