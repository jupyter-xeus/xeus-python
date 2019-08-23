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

#include "xeus/xsystem.hpp"

#include "gtest/gtest.h"
#include "xeus_client.hpp"

#include "pybind11/pybind11.h"

#include <stdio.h>
#ifdef WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

/***********************************
 * Should be moved in a utils file *
 ***********************************/

std::string get_current_working_directory()
{
    char buff[FILENAME_MAX];
    char* r = getcwd(buff, FILENAME_MAX);
    // Avoids warning
    (void*)r;
    std::string current_dir(buff);
    //current_dir += '/';
#ifdef WIN32
    std::replace(current_dir.begin(), current_dir.end(), '\\', '/');
#endif
    return current_dir;
}

namespace nl = nlohmann;
using namespace std::chrono_literals;

/***********************
 * Predefined messages *
 ***********************/

nl::json make_attach_request(int seq)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "attach"},
        {"arguments", {
            {"justMyCode", false},
            {"cwd", get_current_working_directory()}
        }}
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

nl::json make_breakpoint_request(int seq, const std::string& path, int line_number1, int line_number2)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "setBreakpoints"},
        {"arguments", {
            {"source", {
                {"path", path}
            }},
            {"breakpoints", 
                nl::json::array({nl::json::object({{"line", line_number1}}), nl::json::object({{"line", line_number2}})})
            },
            {"lines", {line_number1, line_number2}},
            {"sourceModified", false}
        }}
    };
    return req;
}

nl::json make_configuration_done_request(int seq)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "configurationDone"},
    };
    return req;
}

nl::json make_next_request(int seq, int thread_id)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "next"},
        {"arguments", {
            {"threadId", thread_id}
        }}
    };
    return req;
}

nl::json make_continue_request(int seq, int thread_id)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "continue"},
        {"arguments", {
            {"threadId", thread_id}
        }}
    };
    return req;
}

nl::json make_evaluate_request(int seq, const std::string& code)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "evaluate"},
        {"arguments", {
            {"expression", code}
        }}
    };
    return req;
}

nl::json make_stacktrace_request(int seq, int thread_id)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "stackTrace"},
        {"arguments", {
            {"threadId", thread_id}
        }}
    };
    return req;
}

nl::json make_scopes_request(int seq, int frame_id)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "scopes"},
        {"arguments", {
            {"frameId", frame_id}
        }}
    };
    return req;
}

nl::json make_variables_request(int seq, int var_ref)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "variables"},
        {"arguments", {
            {"variablesReference", var_ref}
        }}
    };
    return req;
}

nl::json make_execute_request(const std::string& code)
{
    nl::json req = {
        {"code", code},
    };
    return req;
}

nl::json make_update_cell_request(int seq, int cell_id, int next_id, const std::string& code)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "updateCell"},
        {"arguments", {
            {"currentId", cell_id},
            {"nextId", next_id},
            {"code", code}
        }}
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
    bool test_attach();
    bool test_external_set_breakpoints();
    bool test_external_next_continue();
    bool test_set_breakpoints();
    bool test_next_continue();
    void shutdown();

private:

    nl::json attach();
    nl::json set_external_breakpoints();
    nl::json set_breakpoints();
    
    std::string get_external_path();
    void dump_external_file();

    std::string make_code() const;
    std::string make_external_code() const;
    std::string make_external_invoker_code() const;

    bool print_code_variable(const std::string& expected, int& seq);
    void next(int& seq);
    void continue_exec(int& seq);

    bool next_continue_common();

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

bool debugger_client::test_attach()
{
    nl::json rep = attach();
    return rep["content"]["success"].get<bool>();
}

bool debugger_client::test_external_set_breakpoints()
{
    attach();
    nl::json rep = set_external_breakpoints();
    return rep["content"]["body"].size() != 0;
}

bool debugger_client::print_code_variable(const std::string& expected, int& seq)
{
    m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
    ++seq;
    nl::json json1 = m_client.receive_on_control();
    
    int frame_id = json1["content"]["body"]["stackFrames"][0]["id"].get<int>();
    m_client.send_on_control("debug_request", make_scopes_request(seq, frame_id));
    ++seq;
    nl::json json2 = m_client.receive_on_control();

    int variable_ref = json2["content"]["body"]["scopes"][0]["variablesReference"].get<int>();
    m_client.send_on_control("debug_request", make_variables_request(seq, variable_ref));
    ++seq;
    nl::json json3 = m_client.receive_on_control();

    const auto& ar = json3["content"]["body"]["variables"];
    bool var_found = false;
    std::string name, value;
    for(auto it = ar.begin(); it != ar.end() && !var_found; ++it)
    {
        auto d = *it;
        name = d["name"];
        if(name == "i")
        {
            var_found = true;
            value = d["value"];
        }
    }

    std::cout << "Variable " << name << " = " << value << std::endl;
    return value == expected;
}

bool debugger_client::test_set_breakpoints()
{
    attach();
    nl::json rep = set_breakpoints();
    return rep["content"]["body"].size() != 0;
}

void debugger_client::next(int& seq)
{
    m_client.send_on_control("debug_request", make_next_request(seq, 1));
    m_client.receive_on_control();
    ++seq;
}

void debugger_client::continue_exec(int& seq)
{
    m_client.send_on_control("debug_request", make_continue_request(seq, 1));
    m_client.receive_on_control();
    ++seq;
}

bool debugger_client::next_continue_common()
{
    // TODO: remove this, handle events
    std::this_thread::sleep_for(1s);

    // Code should have stopped line 3
    int seq = 6;
    std::cout << "Thread hit a breakpoint line 3" << std::endl;
    bool res = print_code_variable("4", seq);

    next(seq);

    // TODO: remove this, handle events
    std::this_thread::sleep_for(1s);
    
    // Code should have stopped line 4
    std::cout << "Thread is stopped line 4" << std::endl;
    res = print_code_variable("8", seq) && res;

    continue_exec(seq);

    // Code should have stopped line 5
    std::cout << "Thread hit a breakpoint line 5" << std::endl;
    res = print_code_variable("11", seq) && res;

    continue_exec(seq);

    nl::json rep = m_client.receive_on_shell();
    return rep["content"]["status"] == "ok";
}

bool debugger_client::test_external_next_continue()
{
    attach();
    // We set 2 breakpoints on line 3 and 5 of external_code.py
    set_external_breakpoints();
    m_client.send_on_control("debug_request", make_configuration_done_request(5));
    m_client.receive_on_control();

    m_client.send_on_shell("execute_request", make_execute_request(make_external_invoker_code()));
    return next_continue_common();
}

bool debugger_client::test_next_continue()
{
    attach();
    // We set 2 breakpoints on line 2 and 4 of the first cell
    set_breakpoints();
    m_client.send_on_control("debug_request", make_configuration_done_request(5));
    m_client.receive_on_control();

    m_client.send_on_shell("execute_request", make_execute_request(make_code()));
    return next_continue_common();
}

void debugger_client::shutdown()
{
    m_client.send_on_control("shutdown_request", make_shutdown_request());
    m_client.receive_on_control();
}

nl::json debugger_client::attach()
{
    m_client.send_on_control("debug_request", make_init_request());
    m_client.receive_on_control();
    m_client.send_on_control("debug_request", make_attach_request(3));
    return m_client.receive_on_control();
}

// Dump a python file and set breakpoints in it
nl::json debugger_client::set_external_breakpoints()
{
    dump_external_file();
    std::string path = get_external_path();
    m_client.send_on_control("debug_request", make_breakpoint_request(4, path, 3, 5));
    return m_client.receive_on_control();
}

nl::json debugger_client::set_breakpoints()
{
    m_client.send_on_control("debug_request", make_update_cell_request(4, 0, 1, make_code()));
    nl::json res = m_client.receive_on_control();
    std::string path = res["content"]["body"]["sourcePath"].get<std::string>();
    m_client.send_on_control("debug_request", make_breakpoint_request(4, path, 2, 4));
    return m_client.receive_on_control();
}

std::string debugger_client::get_external_path()
{
    return get_current_working_directory() + "/external_code.py";
}

void debugger_client::dump_external_file()
{
    static bool already_dumped = false;
    if(!already_dumped)
    {
        std::ofstream out(get_external_path());
        out << make_external_code() << std::endl;
        already_dumped = true;
    }
}

std::string debugger_client::make_code() const
{
    return "i=4\ni+=4\ni+=3\ni";
}

std::string debugger_client::make_external_code() const
{
    return "def my_test():\n\ti=4\n\ti+=4\n\ti+=3\n\treturn i\n";
}

std::string debugger_client::make_external_invoker_code() const
{
    return "import external_code as ec\nec.my_test()\n";
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

void start_kernel()
{
    dump_connection_file();
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

TEST(debugger, attach)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_attach.log");
        bool res = deb.test_attach();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}

#if PY_MAJOR_VERSION == 3
TEST(debugger, set_external_breakpoints)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_set_external_breakpoints.log");
        bool res = deb.test_external_set_breakpoints();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}

TEST(debugger, external_next_continue)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_external_next_continue.log");
        bool res = deb.test_external_next_continue();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}

TEST(debugger, set_breakpoints)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_set_breakpoints.log");
        bool res = deb.test_set_breakpoints();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}

TEST(debugger, next_continue)
{
    start_kernel();
    zmq::context_t context;
    {
        debugger_client deb(context, KERNEL_JSON, "debugger_next_continue.log");
        bool res = deb.test_next_continue();
        deb.shutdown();
        std::this_thread::sleep_for(2s);
        EXPECT_TRUE(res);
    }
}
#endif

