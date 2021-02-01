/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "xeus-python/xutils.hpp"
#include "xeus-python/xtraceback.hpp"

#include "pybind11/pybind11.h"

#include "xinternal_utils.hpp"

namespace py = pybind11;

namespace xpyt
{
    std::string highlight(const std::string& code)
    {
        py::object py_highlight = py::module::import("pygments").attr("highlight");
        // py::module::import("pygments").attr("formatters") does NOT work due
        // to side effects when importing pygments
        py::object formatter = py::module::import("pygments.formatters").attr("TerminalFormatter");

        py::object lexer = py::module::import("pygments.lexers").attr("Python3Lexer");

        return py::str(py_highlight(code, lexer(), formatter()));
    }

    std::string extract_line(const std::string& code, std::size_t lineno)
    {
        std::istringstream code_stream(code);
        std::string line;

        for (std::size_t line_number(0); line_number < lineno; line_number++)
        {
            std::getline(code_stream, line);
        }

        // Remove indentation
        if (!line.empty())
        {
            std::size_t idx(0);
            while (line[idx] == ' ')
            {
                idx++;
            }
            line = line.substr(idx);
        }

        return line;
    }

    using filename_map = std::map<std::string, int>;

    filename_map& get_filename_map()
    {
        static filename_map fnm;
        return fnm;
    }

    void register_filename_mapping(const std::string& filename, int execution_count)
    {
        get_filename_map()[filename] = execution_count;
    }

    xerror extract_error(const py::object& type, const py::object& value, const py::object& traceback)
    {
        xerror out;

        // This should NOT happen
        if (type.is_none())
        {
            out.m_ename = "Error";
            out.m_evalue = "";
        }
        else
        {
            out.m_ename = py::str(type.attr("__name__"));
            out.m_evalue = py::str(value);

            std::size_t first_frame_size(75);
            std::string delimiter(first_frame_size, '-');
            std::string traceback_msg("Traceback (most recent call last)");
            std::string first_frame_padding(first_frame_size - traceback_msg.size() - out.m_ename.size(), ' ');

            std::stringstream first_frame;
            first_frame << red_text(delimiter) << "\n"
                        << red_text(out.m_ename) << first_frame_padding << traceback_msg;
            out.m_traceback.push_back(first_frame.str());

            if (traceback.ptr() != nullptr && !traceback.is_none())
            {
                for (py::handle py_frame : py::module::import("traceback").attr("extract_tb")(traceback))
                {
                    std::string filename;
                    std::string lineno;
                    std::string name;
                    std::string line;

                    filename = py::str(py_frame.attr("filename"));
                    lineno = py::str(py_frame.attr("lineno"));
                    name = py::str(py_frame.attr("name"));
                    line = py::str(py_frame.attr("line"));

                    // Workaround for py::exec
                    if (filename == "<string>")
                    {
                        continue;
                    }

                    std::stringstream cpp_frame;
                    std::string padding(6 - lineno.size(), ' ');
                    std::string file_prefix;
                    std::string func_name;

                    std::string prefix = get_tmp_prefix();
                    // If the error occured in a cell code, extract the line from the given code
                    if(!filename.empty() && !filename.compare(0, prefix.size(), prefix.c_str(), prefix.size()))
                    {
                        file_prefix = "In  ";
                        auto it = get_filename_map().find(filename);
                        if(it != get_filename_map().end())
                        {
                            filename = '[' + std::to_string(it->second) + ']';
                        }
                    }
                    else
                    {
                        file_prefix = "File ";
                        func_name = ", in " + green_text(name);
                    }

                    cpp_frame << file_prefix << blue_text(filename) << func_name << ":\n"
                            << "Line " << blue_text(lineno) << ":"
                            << padding << highlight(line);
                    out.m_traceback.push_back(cpp_frame.str());
                }
            }

            std::stringstream last_frame;
            last_frame << red_text(out.m_ename) << ": " << out.m_evalue << "\n"
                       << red_text(delimiter);
            out.m_traceback.push_back(last_frame.str());
        }

        return out;
    }

    xerror extract_error(py::error_already_set& error)
    {
        return extract_error(error.type(), error.value(), error.trace());
    }

    /********************
     * traceback module *
     ********************/

    py::module get_traceback_module_impl()
    {
        py::module traceback_module = create_module("traceback");

        traceback_module.def("register_filename_mapping",
            register_filename_mapping,
            py::arg("filename"),
            py::arg("execution_count")
        );

        exec(py::str(R"(
last_error = None


def reset_last_error():
    global last_error

    last_error = None


def set_last_error(type, value, traceback):
    global last_error

    last_error = (type, value, traceback)


def get_last_error():
    global last_error

    return last_error
        )"), traceback_module.attr("__dict__"));

        return traceback_module;
    }

    py::module get_traceback_module()
    {
        static py::module traceback_module = get_traceback_module_impl();
        return traceback_module;
    }
}
