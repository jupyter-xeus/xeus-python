/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "doctest/doctest.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

#include "xeus/xsystem.hpp"

#include "xeus_client.hpp"

#include "xeus-zmq/xzmq_context.hpp"

#include "pybind11/pybind11.h"

#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <boost/process.hpp>

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
#ifdef _WIN32
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

nl::json make_breakpoint_request(int seq, const std::string& path, int line_number)
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
                nl::json::array({nl::json::object({{"line", line_number}})})
            },
            {"lines", {line_number}},
            {"sourceModified", false}
        }}
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

nl::json make_variables_request(int seq, int var_ref, int start = 0, int count = 0)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "variables"},
        {"arguments", {
            {"variablesReference", var_ref},
            {"start", start},
            {"count", count}
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

nl::json make_dump_cell_request(int seq, const std::string& code)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "dumpCell"},
        {"arguments", {
            {"code", code}
        }}
    };
    return req;
}

nl::json make_source_request(int seq, const std::string& path)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "source"},
        {"arguments", {
            {"source", {
                {"path", path}
            }}
        }}
    };
    return req;
}

nl::json make_stepin_request(int seq, int thread_id)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "stepIn"},
        {"arguments", {
            {"threadId", thread_id}
        }}
    };
    return req;
}

nl::json make_debug_info_request(int seq)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "debugInfo"}
    };
    return req;
}

nl::json make_inspect_variables_request(int seq)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "inspectVariables"}
    };
    return req;
}

nl::json make_rich_inspect_variables_request(int seq, const std::string& var_name)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "richInspectVariables"},
        {"arguments", {
            {"variableName", var_name},
        }}
    };
    return req;
}

nl::json make_rich_inspect_variables_request(int seq, const std::string& var_name, int frame_id)
{
    nl::json req = make_rich_inspect_variables_request(seq, var_name);
    req["arguments"]["frameId"] = frame_id;
    return req;
}

nl::json make_copy_to_globals_request(int seq, const std::string& src_var_name, const std::string& dst_var_name, int src_frame_id)
{
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "copyToGlobals"},
        {"arguments", {
            {"srcVariableName", src_var_name},
            {"dstVariableName", dst_var_name},
            {"srcFrameId", src_frame_id}
        }}
    };
    return req;
}

nl::json make_exception_breakpoint_request(int seq)
{
    nl::json except_option = {
        {"path", nl::json::array({
                nl::json::object({{"names", nl::json::array({"Python Exceptions"})}})
        })},
        {"breakMode", "always"}
    };
    nl::json options = nl::json::array({except_option, except_option});
    nl::json req = {
        {"type", "request"},
        {"seq", seq},
        {"command", "setExceptionBreakpoints"},
        {"arguments", {
            {"filters", {"raised", "uncaught"}},
            {"exceptionOptions", options}
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

    debugger_client(xeus::xcontext& context,
                    const std::string& connection_file,
                    const std::string& log_file);

    bool test_init();
    bool test_disconnect();
    bool test_attach();
    bool test_external_set_breakpoints();
    bool test_external_next_continue();
    bool test_set_breakpoints();
    bool test_set_exception_breakpoints();
    bool test_source();
    bool test_next_continue();
    bool test_step_in();
    bool test_stack_trace();
    bool test_debug_info();
    bool test_inspect_variables();
    bool test_rich_inspect_variables();
    bool test_variables();
    bool test_copy_to_globals();
    void start();
    void shutdown();
    void disconnect_debugger();

private:

    nl::json attach();
    nl::json set_external_breakpoints();
    nl::json set_breakpoints();
    nl::json set_exception_breakpoints();

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

debugger_client::debugger_client(xeus::xcontext& context,
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
    return rep["content"]["success"].get<bool>();
}

bool debugger_client::test_disconnect()
{
    attach();
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

    if(json1["content"]["body"]["stackFrames"].empty())
    {
        m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
        ++seq;
        json1 = m_client.receive_on_control();
    }

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

bool debugger_client::test_set_exception_breakpoints()
{
    attach();
    nl::json reply = set_exception_breakpoints();
    std::string code = "a = 3 * undef";
    m_client.send_on_shell("execute_request", make_execute_request(code));

    nl::json ev = m_client.wait_for_debug_event("stopped");
    std::string reason = ev["content"]["body"]["reason"].get<std::string>();

    return reason == "exception";
}

bool debugger_client::test_source()
{
    attach();

    std::string code = make_code();
    m_client.send_on_control("debug_request", make_dump_cell_request(4, code));
    nl::json reply = m_client.receive_on_control();
    std::string path = reply["content"]["body"]["sourcePath"].get<std::string>();

    m_client.send_on_control("debug_request", make_source_request(5, path));
    nl::json rep = m_client.receive_on_control();

    nl::json source = rep["content"]["body"]["content"];
    bool res = source == code;
    return res;
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
    nl::json ev = m_client.wait_for_debug_event("stopped");

    // Code should have stopped line 3
    int seq = ev["content"]["seq"].get<int>();
    std::cout << "Thread hit a breakpoint line 3" << std::endl;
    bool res = print_code_variable("4", seq);

    next(seq);
    m_client.wait_for_debug_event("stopped");

    // Code should have stopped line 4
    std::cout << "Thread is stopped line 4" << std::endl;
    res = print_code_variable("8", seq) && res;

    continue_exec(seq);
    m_client.wait_for_debug_event("stopped");

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
    std::cout << "Configuration done sent" << std::endl;
    m_client.receive_on_control();
    std::cout << "Configuration done received" << std::endl;

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

bool debugger_client::test_step_in()
{
    attach();

    m_client.send_on_control("debug_request", make_dump_cell_request(4, make_external_invoker_code()));
    nl::json dump_res = m_client.receive_on_control();
    std::string path = dump_res["content"]["body"]["sourcePath"].get<std::string>();
    m_client.send_on_control("debug_request", make_breakpoint_request(4, path, 1, 2));
    m_client.receive_on_control();

    m_client.send_on_control("debug_request", make_configuration_done_request(5));
    m_client.receive_on_control();

    m_client.send_on_shell("execute_request", make_execute_request(make_external_code()));
    m_client.receive_on_shell();

    m_client.send_on_shell("execute_request", make_execute_request(make_external_invoker_code()));

    nl::json ev = m_client.wait_for_debug_event("stopped");
    int seq = ev["content"]["seq"].get<int>();
    // Code should have stopped line 1
    std::cout << "Thread hit a breakpoint line 1" << std::endl;
    continue_exec(seq);
    m_client.wait_for_debug_event("stopped");

    // Code should have stopped line 2
    std::cout << "Thread hit a breakpoint line 2" << std::endl;
    m_client.send_on_control("debug_request", make_stepin_request(seq, 1));
    m_client.receive_on_control();
    m_client.wait_for_debug_event("stopped");
    seq = ev["content"]["seq"].get<int>();
    std::cout << "Thread has stepped in" << std::endl;
    next(seq);
    m_client.wait_for_debug_event("stopped");

    bool res = print_code_variable("4", seq);
    next(seq);
    m_client.wait_for_debug_event("stopped");

    res = print_code_variable("8", seq) && res;

    continue_exec(seq);

    nl::json rep = m_client.receive_on_shell();
    res = rep["content"]["status"] == "ok" && res;

    return res;
}

bool debugger_client::test_stack_trace()
{
    attach();
    // We set 2 breakpoints on line 2 and 4 of the first cell
    set_breakpoints();
    m_client.send_on_control("debug_request", make_configuration_done_request(5));
    m_client.receive_on_control();

    m_client.send_on_shell("execute_request", make_execute_request(make_code()));
    nl::json ev = m_client.wait_for_debug_event("stopped");

    int seq = 6;
    m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
    ++seq;
    nl::json stackframes = m_client.receive_on_control();

    bool res = stackframes["content"]["body"]["stackFrames"].size() != 0;
    continue_exec(seq);
    m_client.wait_for_debug_event("stopped");
    continue_exec(seq);
    return res;
}

bool debugger_client::test_debug_info()
{
    attach();

    m_client.send_on_control("debug_request", make_debug_info_request(4));
    nl::json rep1 = m_client.receive_on_control();

    bool res = rep1["content"]["body"]["breakpoints"].size() == 0;
    set_breakpoints();
    set_external_breakpoints();

    m_client.send_on_control("debug_request", make_configuration_done_request(5));
    m_client.receive_on_control();

    m_client.send_on_control("debug_request", make_debug_info_request(12));
    nl::json rep2 = m_client.receive_on_control();

    nl::json bp_list = rep2["content"]["body"]["breakpoints"];
    res = res && bp_list.size() == 2;
    res = res && bp_list[0]["breakpoints"].size() == 2 && bp_list[1]["breakpoints"].size() == 2;

    nl::json stopped_list = rep2["content"]["body"]["stoppedThreads"];
    res = res && stopped_list.size() == 0;

    m_client.send_on_shell("execute_request", make_execute_request(make_code()));
    m_client.wait_for_debug_event("stopped");
    m_client.send_on_control("debug_request", make_debug_info_request(14));
    nl::json rep3 = m_client.receive_on_control();

    nl::json stopped_list2 = rep3["content"]["body"]["stoppedThreads"];

    int seq = 15;
    next(seq);
    m_client.wait_for_debug_event("stopped");

    continue_exec(seq);
    m_client.wait_for_debug_event("stopped");

    continue_exec(seq);

    nl::json rep4 = m_client.receive_on_shell();
    bool res2 = rep4["content"]["status"] == "ok";
    return res && res2 && stopped_list2[0] == 1;
}

bool debugger_client::test_inspect_variables()
{
    attach();

    std::string code = "i=4\nj=i+4\nk=j-3\n";
    m_client.send_on_shell("execute_request", make_execute_request(code));
    m_client.receive_on_shell();

    m_client.send_on_control("debug_request", make_inspect_variables_request(0));
    nl::json rep = m_client.receive_on_control();

    nl::json vars = rep["content"]["body"]["variables"];

    auto check_var = [&vars](const std::string& name, const std::string& value) {
        auto x = std::find_if(vars.begin(), vars.end(), [&name](const nl::json& var) {
            return var.is_object() && var.value("name", "") == name;
        });
        if (x == vars.end())
        {
            std::cout << "missing " << name << std::endl;
            return false;
        }
        nl::json var = *x;
        return var["value"] == value && var["variablesReference"] == 0;
    };

    bool res = check_var("i", "4") && check_var("j", "8") && check_var("k", "5");
    return res;
}

std::string rich_inspect_class_def = R"RICH(
class Person:
    def __init__(self, name="John Doe", address="Paris", picture=""):
        self.name = name
        self.address = address
        self.picture = picture

    def _repr_mimebundle_(self, include=None, exclude=None):
        return {
            "text/html": """<img src="{}">
                  <div><i class='fa-user fa'></i>: {}</div>
                  <div><i class='fa-map fa'></i>: {}</div>""".format(self.picture, self.name, self.address)
        }
)RICH";

std::string rich_html = R"RICH(<img src="">
                  <div><i class='fa-user fa'></i>: James Smith</div>
                  <div><i class='fa-map fa'></i>: Boston</div>)RICH";
std::string rich_html2 = R"RICH(<img src="">
                  <div><i class='fa-user fa'></i>: John Smith</div>
                  <div><i class='fa-map fa'></i>: NYC</div>)RICH";

bool debugger_client::test_rich_inspect_variables()
{
    bool res = false;
    {
        m_client.send_on_shell("execute_request", make_execute_request(rich_inspect_class_def));
        m_client.receive_on_shell();

        std::string code = "james = Person(\"James Smith\", \"Boston\")";
        m_client.send_on_shell("execute_request", make_execute_request(code));
        m_client.receive_on_shell();

        m_client.send_on_control("debug_request", make_rich_inspect_variables_request(0, "james"));
        nl::json rep = m_client.receive_on_control();
        nl::json data = rep["content"]["body"]["data"];
        std::string html = data["text/html"].get<std::string>();
        std::string plain = data["text/plain"].get<std::string>();
        res = html == rich_html && !plain.empty();
    }
    bool res2 = true;
    {
        attach();
        std::string code = "john = Person(\"John Smith\", \"NYC\")\ni = 4\nj = 2";
        m_client.send_on_shell("execute_request", make_execute_request(code));
        m_client.receive_on_shell();

        m_client.send_on_control("debug_request", make_dump_cell_request(12, code));
        nl::json dump_res = m_client.receive_on_control();
        std::string path = dump_res["content"]["body"]["sourcePath"].get<std::string>();
        m_client.send_on_control("debug_request", make_breakpoint_request(12, path, 2));
        m_client.receive_on_control();

        m_client.send_on_shell("execute_request", make_execute_request(code));

        nl::json ev = m_client.wait_for_debug_event("stopped");

        int seq = 14;
        m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
        nl::json stackframes = m_client.receive_on_control();
        int frame_id = stackframes["content"]["body"]["stackFrames"][0]["id"].get<int>();
        m_client.send_on_control("debug_request", make_rich_inspect_variables_request(seq, "john", frame_id));
        nl::json rep = m_client.receive_on_control();
        m_client.send_on_control("debug_request", make_continue_request(seq, 1));
        m_client.receive_on_control();
        m_client.receive_on_shell();

        nl::json data = rep["content"]["body"]["data"];
        std::string html = data["text/html"].get<std::string>();
        std::string plain = data["text/plain"].get<std::string>();
        res = html == rich_html2 && !plain.empty();

    }
    return res && res2;
}

bool debugger_client::test_variables()
{
    std::string code = "i=4\nj=i+4\nk=j-3\ni=k+2\n";
    attach();
    {
        m_client.send_on_control("debug_request", make_dump_cell_request(4, code));
        nl::json res = m_client.receive_on_control();
        std::string path = res["content"]["body"]["sourcePath"].get<std::string>();
        m_client.send_on_control("debug_request", make_breakpoint_request(4, path, 4));
        m_client.receive_on_control();
    }

    m_client.send_on_control("debug_request", make_configuration_done_request(5));
    m_client.receive_on_control();

    m_client.send_on_shell("execute_request", make_execute_request(code));
    nl::json ev = m_client.wait_for_debug_event("stopped");
    int seq = ev["content"]["seq"].get<int>();

    m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
    ++seq;
    nl::json json1 = m_client.receive_on_control();

    if(json1["content"]["body"]["stackFrames"].empty())
    {
        m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
        ++seq;
        json1 = m_client.receive_on_control();
    }

    int frame_id = json1["content"]["body"]["stackFrames"][0]["id"].get<int>();
    m_client.send_on_control("debug_request", make_scopes_request(seq, frame_id));
    ++seq;
    nl::json json2 = m_client.receive_on_control();

    int variable_ref = json2["content"]["body"]["scopes"][0]["variablesReference"].get<int>();
    m_client.send_on_control("debug_request", make_variables_request(seq, variable_ref, 1, 1));
    ++seq;
    nl::json json3 = m_client.receive_on_control();

    const auto& ar = json3["content"]["body"]["variables"];
    bool res = ar.size() == 1u;

    m_client.send_on_control("debug_request", make_continue_request(seq, 1));
    m_client.receive_on_control();

    return res;
}

bool debugger_client::test_copy_to_globals()
{
    std::string local_var_name = "var";
    std::string global_var_name = "var_copy";
    std::string code = "from IPython.core.display import HTML\ndef my_test():\n\t" + local_var_name + " = HTML(\"<p>test content</p>\")\n\tpass\nmy_test()";
    int seq = 12;

    // Init debugger and set breakpoint
    attach();
    m_client.send_on_control("debug_request", make_dump_cell_request(seq, code));
    ++seq;
    nl::json dump_res = m_client.receive_on_control();
    std::string path = dump_res["content"]["body"]["sourcePath"].get<std::string>();
    m_client.send_on_control("debug_request", make_breakpoint_request(seq, path, 4));
    ++seq;
    m_client.receive_on_control();

    // Execute code
    m_client.send_on_shell("execute_request", make_execute_request(code));

    // Get breakpoint event
    nl::json ev = m_client.wait_for_debug_event("stopped");
    m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
    ++seq;
    nl::json json1 = m_client.receive_on_control();
    if(json1["content"]["body"]["stackFrames"].empty())
    {
        m_client.send_on_control("debug_request", make_stacktrace_request(seq, 1));
        ++seq;
        json1 = m_client.receive_on_control();
    }

    // Get local frame id
    int frame_id = json1["content"]["body"]["stackFrames"][0]["id"].get<int>();

    // Copy the variable
    m_client.send_on_control("debug_request", make_copy_to_globals_request(seq, local_var_name, global_var_name, frame_id));
    ++seq;

    // Get the scopes
    nl::json reply = m_client.receive_on_control();
    m_client.send_on_control("debug_request", make_scopes_request(seq, frame_id));
    ++seq;
    nl::json json2 = m_client.receive_on_control();

    // Get the local variable
    int variable_ref_local = json2["content"]["body"]["scopes"][0]["variablesReference"].get<int>();
    m_client.send_on_control("debug_request", make_variables_request(seq, variable_ref_local));
    ++seq;
    nl::json json3 = m_client.receive_on_control();
    nl::json local_var = {};
    for (auto &var: json3["content"]["body"]["variables"]){
        if (var["evaluateName"] == local_var_name) {
            local_var = var;
        }
    }
    if (!local_var.contains("evaluateName")) {
        std::cout << "Local variable \"" + local_var_name + "\" not found";
        return false;
    }

    // Get the global variable (copy of the local variable)
    nl::json global_scope = json2["content"]["body"]["scopes"].back();
    int variable_ref_global = global_scope["variablesReference"].get<int>();
    m_client.send_on_control("debug_request", make_variables_request(seq, variable_ref_global));
    ++seq;
    nl::json json4 = m_client.receive_on_control();
    nl::json global_var = {};
    for (auto &var: json4["content"]["body"]["variables"]){
        if (var["evaluateName"] == global_var_name) {
            global_var = var;
        }
    }
    if (!global_var.contains("evaluateName")) {
        std::cout << "Global variable \"" + global_var_name + "\" not found";
        return false;
    }

    // Compare local and global variable
    return global_var["value"] == local_var["value"] && global_var["type"] == local_var["type"];
}

void debugger_client::start()
{
    m_client.start();
}

void debugger_client::disconnect_debugger()
{
    m_client.send_on_control("debug_request", make_disconnect_request(INT_MAX));
    m_client.receive_on_control();
}

void debugger_client::shutdown()
{
    if (m_client.kernel_dead)
    {
        throw std::runtime_error("Kernel is not alive. Cannot send shutdown request.");
    }
    m_client.stop_channels();
    m_client.send_on_control("shutdown_request", make_shutdown_request());
    m_client.receive_on_control();
}

nl::json debugger_client::attach()
{
    m_client.send_on_control("debug_request", make_init_request());
    nl::json rep = m_client.receive_on_control();
    if (!rep["content"]["success"].get<bool>())
    {
        shutdown();
        std::this_thread::sleep_for(4s);
        throw std::runtime_error("Could not initialize debugger, exiting");
    }
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
    m_client.send_on_control("debug_request", make_dump_cell_request(4, make_code()));
    nl::json res = m_client.receive_on_control();
    std::string path = res["content"]["body"]["sourcePath"].get<std::string>();
    m_client.send_on_control("debug_request", make_breakpoint_request(4, path, 2, 4));
    return m_client.receive_on_control();
}

nl::json debugger_client::set_exception_breakpoints()
{
    m_client.send_on_control("debug_request", make_exception_breakpoint_request(4));
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
    return "i=4\ni+=4\ni+=3\ni-=1";
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
 * timer *
 *********/

class timer
{
public:

    timer();
    ~timer();

    void notify_done();

private:

    void run_timer();

    std::thread m_runner;
    std::condition_variable m_cv;
    std::mutex m_mcv;
    bool m_done;
};

timer::timer()
    : m_runner()
    , m_cv()
    , m_mcv()
    , m_done(false)
{
    m_runner = std::move(std::thread(&timer::run_timer, this));
}

timer::~timer()
{
    m_runner.join();
}

void timer::notify_done()
{
    {
        std::lock_guard<std::mutex> lk(m_mcv);
        m_done = true;
    }
    m_cv.notify_one();
}

void timer::run_timer()
{
    std::unique_lock<std::mutex> lk(m_mcv);
    if (!m_cv.wait_for(lk, std::chrono::seconds(20), [this]() { return m_done; }))
    {
        std::clog << "Unit test time out !!" << std::endl;
        std::terminate();
    }
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

struct KernelProcess
{
    KernelProcess()
    {
        std::this_thread::sleep_for(2s);
    }

private:

    bool _ = [] { dump_connection_file(); return true; }();
    boost::asio::io_context ctx;
    boost::process::process process{ ctx, boost::process::environment::find_executable("xpython"), { "-f" , KERNEL_JSON } };
};

TEST_SUITE("debugger")
{
    TEST_CASE("init")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_init.log");
            deb.start();
            bool res = deb.test_init();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("disconnect")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_disconnect.log");
            deb.start();
            bool res = deb.test_disconnect();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("attach")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_attach.log");
            deb.start();
            bool res = deb.test_attach();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("multisession")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_multi_session.log");
            deb.start();
            bool res1 = deb.test_disconnect();
            std::this_thread::sleep_for(4s);
            bool res2 = deb.test_disconnect();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res1);
            CHECK(res2);
            t.notify_done();
        }
    }

    TEST_CASE("set_external_breakpoints")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_set_external_breakpoints.log");
            deb.start();
            bool res = deb.test_external_set_breakpoints();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    // TODO
    /*
    TEST_CASE("external_next_continue")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_external_next_continue.log");
            deb.start();
            bool res = deb.test_external_next_continue();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }
    */
    TEST_CASE("set_breakpoints")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_set_breakpoints.log");
            deb.start();
            bool res = deb.test_set_breakpoints();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("set_exception_breakpoints")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_set_exception_breakpoints.log");
            deb.start();
            bool res = deb.test_set_exception_breakpoints();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(5s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("source")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_source.log");
            deb.start();
            bool res = deb.test_source();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    // TODO
    /*
    TEST_CASE("next_continue")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_next_continue.log");
            deb.start();
            bool res = deb.test_next_continue();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }
    */

    // TODO
    /*
    TEST_CASE("stepin")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_stepin.log");
            deb.start();
            bool res = deb.test_step_in();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }
    */

    TEST_CASE("stack_trace")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_stack_trace.log");
            deb.start();
            bool res = deb.test_stack_trace();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("debug_info")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_debug_info.log");
            deb.start();
            bool res = deb.test_debug_info();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("inspect_variables")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_inspect_variables.log");
            deb.start();
            bool res = deb.test_inspect_variables();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

// TODO: Get test_rich_inspect_variables to work
/*
    TEST_CASE("rich_inspect_variables")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_rich_inspect_variables.log");
            deb.start();
            bool res = deb.test_rich_inspect_variables();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }
*/

    TEST_CASE("variables")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_variables.log");
            deb.start();
            bool res = deb.test_variables();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }

    TEST_CASE("copy_to_globals")
    {
        KernelProcess xpython_process;
        timer t;
        auto context_ptr = xeus::make_zmq_context();
        {
            debugger_client deb(*context_ptr, KERNEL_JSON, "debugger_copy_to_globals.log");
            deb.start();
            bool res = deb.test_copy_to_globals();
            deb.disconnect_debugger();
            deb.shutdown();
            std::this_thread::sleep_for(4s);
            CHECK(res);
            t.notify_done();
        }
    }
}