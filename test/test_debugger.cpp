/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>

#include "gtest/gtest.h"
#include "xeus_client.hpp"

namespace nl = nlohmann;
using namespace std::chrono_literals;

/***********************
 * Predefined messages *
 ***********************/

nl::json make_attach_request()
{
    nl::json req = {
        {"type", "request"},
        {"seq", 1},
        {"command", "attach"}
    };
    return req;
}

nl::json make_init_request()
{
    nl::json req = {
        {"type", "request"},
        {"seq", 1},
        {"command", "initialize"},
        {"arguments", {
            {"cliendID", "vscode"},
            {"clientName", "Visual Studio Code"},
            {"adapterID", "python"},
            {"pathFormat", "path"},
            {"linesStartAt1", true},
            {"columnsStartAt1", true},
            {"supportsVariableType", true},
            {"supportsVariablePaging", true},
            {"supportsRunInTerminalRequest", false},
            {"locale", "en-us"}
        }}
    };
    return req;
}

nl::json make_disconnect_request(int seq)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "disconnect"},
        {"arguments", {
            {"restart", false},
            {"terminateDebuggee", false}
        }}
    };
    return req;
}

nl::json make_shutdown_request()
{
    nl::json req = {
        {"restart", false}
    };
    return req;
}

/*******************
 * debugger_client *
 *******************/

class debugger_client
{
public:

    debugger_client(zmq::context_t& context,
                    const std::string& connection_file,
                    const std::string& log_file);

    bool test_init();
    bool test_disconnect();
    void shutdown();

private:

    xeus_logger_client m_client;
};

debugger_client::debugger_client(zmq::context_t& context,
                                 const std::string& connection_file,
                                 const std::string& log_file)
    : m_client(context, "debugger_client",
               xeus::load_configuration(connection_file), log_file)
{
}

bool debugger_client::test_init()
{
    m_client.send_on_control("debug_request", make_init_request());
    nl::json rep = m_client.receive_on_control();
    return rep["content"]["type"] == "response";
}

bool debugger_client::test_disconnect()
{
    m_client.send_on_control("debug_request", make_init_request());
    m_client.receive_on_control();
    m_client.send_on_control("debug_request", make_disconnect_request(3));
    nl::json rep = m_client.receive_on_control();
    return rep["content"]["type"] == "response";
}

void debugger_client::shutdown()
{
    m_client.send_on_control("shutdown_request", make_shutdown_request());
    m_client.receive_on_control();
}

/*********
 * tests *
 *********/

namespace
{
    const std::string KERNEL_JSON = "kernel-debug.json";
}

void dump_connection_file()
{
    static bool dumped = false;
    static std::string connection_file = R"(
{
  "shell_port": 60779,
  "iopub_port": 55691,
  "stdin_port": 56973,
  "control_port": 56505,
  "hb_port": 45551,
  "ip": "127.0.0.1",
  "key": "6ef0855c-5cba319b6d05552c44a8ac90",
  "transport": "tcp",
  "signature_scheme": "hmac-sha256",
  "kernel_name": "xcpp"
}
        )";
    if(!dumped)
    {
        std::ofstream out(KERNEL_JSON);
        out << connection_file;
        dumped = true;
    }
}

int start_kernel()
{
    dump_connection_file();
    std::string mkdir = "mkdir xpython_debug_logs";
    int ret = std::system(mkdir.c_str());
    std::thread kernel([]()
    {
#if WIN32
        std::string cmd = ".\\..\\xpython -f " + KERNEL_JSON + "&";
#else
        std::string cmd = "./../xpython -f " + KERNEL_JSON + "&";
#endif
        int ret2 = std::system(cmd.c_str());
    });
    std::this_thread::sleep_for(2s);
    kernel.detach();
    return ret;
}

TEST(debugger, init)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_init.log");
        bool res = deb.test_init();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}

TEST(debugger, disconnect)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_disconnect.log");
        bool res = deb.test_disconnect();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}

TEST(debugger, multisession)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_multi_session.log");
        bool res1 = deb.test_disconnect();
        std::this_thread::sleep_for(2s);
        bool res2 = deb.test_disconnect();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res1);
        EXPECT_TRUE(res2);
    }
}

