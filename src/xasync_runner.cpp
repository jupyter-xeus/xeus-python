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
    }
    
    void xasync_runner::run_impl()
    {

        //the file descriptor for the shell and controller sockets
        // and create (libuv) poll handles to bind them to the loop

        auto  fd_shell = this->get_shell_fd();
        auto fd_controller = this-> get_shell_controller_fd();






        // cast to integers
        int fd_shell_int = static_cast<int>(fd_shell);
        int fd_controller_int = static_cast<int>(fd_controller);




        // wrap this->on_message_doorbell_shell and this->on_message_doorbell_controller
        // into a py::cpp_function
        py::cpp_function shell_callback = py::cpp_function([this]() {
            this->on_message_doorbell_shell();
        });
        py::cpp_function controller_callback = py::cpp_function([this]() {
            this->on_message_doorbell_controller();
        });



        // Or create via exec if you need a more complex function:
        py::exec(R"(
        import asyncio
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
        std::cout << "Shell doorbell received!" << std::endl;
        int ZMQ_DONTWAIT{ 1 }; // from zmq.h 
        while (auto msg = read_shell(ZMQ_DONTWAIT))
        {
            notify_shell_listener(std::move(msg.value()));
        }
    }

    void xasync_runner::on_message_doorbell_controller()
    {
        std::cout << "Controller doorbell received!" << std::endl;
        int ZMQ_DONTWAIT{ 1 }; // from zmq.h
        while (auto msg = read_controller(ZMQ_DONTWAIT))
        {
            std::string val{ msg.value() };
            if (val == "stop")
            {
                send_controller(std::move(val));
                //p_loop->stop(); // TODO
            }
            else
            {
                std::string rep = notify_internal_listener(std::move(val));
                send_controller(std::move(rep));
            }
        }
    }



} // namespace xpyt
