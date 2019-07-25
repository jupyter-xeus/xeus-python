/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <vector>
#include <sstream>
#include <string>

#include "xutils.hpp"
#include "xtraceback.hpp"

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace xpyt
{
    std::string highlight(const std::string& code)
    {
        py::object py_highlight = py::module::import("pygments").attr("highlight");
        // py::module::import("pygments").attr("formatters") does NOT work due
        // to side effects when importing pygments
        py::object formatter = py::module::import("pygments.formatters").attr("TerminalFormatter");

#if PY_MAJOR_VERSION == 2
        py::object lexer = py::module::import("pygments.lexers").attr("PythonLexer");
#else
        py::object lexer = py::module::import("pygments.lexers").attr("Python3Lexer");
#endif

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

    xerror extract_error(py::error_already_set& error, const std::vector<std::string>& inputs)
    {
        xerror out;

        // Fetch the error message, it must be released by the C++ exception first
        error.restore();

        py::object py_type;
        py::object py_value;
        py::object py_tb;
        PyErr_Fetch(&py_type.ptr(), &py_value.ptr(), &py_tb.ptr());

        // This should NOT happen
        if (py_type.is_none())
        {
            out.m_ename = "Error";
            out.m_evalue = error.what();
            out.m_traceback.push_back(error.what());
        }
        else
        {
            out.m_ename = py::str(py_type.attr("__name__"));
            out.m_evalue = py::str(py_value);

            std::size_t first_frame_size(75);
            std::string delimiter(first_frame_size, '-');
            std::string traceback_msg("Traceback (most recent call last)");
            std::string first_frame_padding(first_frame_size - traceback_msg.size() - out.m_ename.size(), ' ');

            std::stringstream first_frame;
            first_frame << red_text(delimiter) << "\n"
                        << red_text(out.m_ename) << first_frame_padding << traceback_msg;
            out.m_traceback.push_back(first_frame.str());

            for (py::handle py_frame : py::module::import("traceback").attr("extract_tb")(py_tb))
            {
                std::string filename;
                std::string lineno;
                std::string name;
                std::string line;

#if PY_MAJOR_VERSION == 2
                py::tuple frame_tuple = py_frame.cast<py::tuple>();
                filename = py::str(frame_tuple[0]);
                lineno = py::str(frame_tuple[1]);
                name = py::str(frame_tuple[2]);
                line = py::str(frame_tuple[3]);
#else
                filename = py::str(py_frame.attr("filename"));
                lineno = py::str(py_frame.attr("lineno"));
                name = py::str(py_frame.attr("name"));
                line = py::str(py_frame.attr("line"));
#endif

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
                    auto pos = filename.find_last_of('[');
                    auto last_pos = filename.find_first_of(']', pos);
                    int exec_count = std::stoi(filename.substr(pos+1, last_pos - pos - 1));
                    filename = filename.substr(pos, last_pos - pos + 1);
                    line = extract_line(inputs.at(exec_count - 1), std::stoi(lineno));
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

            std::stringstream last_frame;
            last_frame << red_text(out.m_ename) << ": " << out.m_evalue << "\n"
                       << red_text(delimiter);
            out.m_traceback.push_back(last_frame.str());
        }

        return out;
    }
}
