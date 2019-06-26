/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XPYT_PTVSD_CLIENT_HPP
#define XPYT_PTVSD_CLIENT_HPP

#include <string>
#include <queue>

#include "zmq.hpp"

namespace xpyt
{
    class xptvsd_client
    {
    public:

        static constexpr const char* HEADER = "Content-Length: ";
        static constexpr size_t HEADER_LENGTH = 16;
        static constexpr const char* SEPARATOR = "\r\n\r\n";
        static constexpr size_t SEPARATOR_LENGTH = 4;

        xptvsd_client(zmq::context_t& context,
                      int socket_linger);


        ~xptvsd_client() = default;

        void start_debugger(std::string ptvsd_end_point,
                            std::string publisher_end_point,
                            std::string controller_end_point);
 
    private:

        void process_message_queue();
        void handle_ptvsd_socket();
        void handle_control_socket();
        void append_tcp_message(std::string& buffer);
        
        zmq::socket_t m_ptvsd_socket;
        std::size_t m_id_size;
        uint8_t m_socket_id[256];

        zmq::socket_t m_publisher;
        zmq::socket_t m_controller;

        bool m_request_stop;

        std::queue<std::string> m_message_queue;
    };
}

#endif
