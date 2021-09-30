/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <algorithm>
#include <map>
#include <vector>
#include <string>

#include "xeus-python/xutils.hpp"
#include "xeus-python/xtraceback.hpp"

#include "pybind11/pybind11.h"

#include "xinternal_utils.hpp"

namespace py = pybind11;

namespace xpyt
{
    py::str get_filename(const py::str& raw_code)
    {
        return get_cell_tmp_file(raw_code);
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

    xerror extract_error(const py::list& error)
    {
        xerror out;

        out.m_ename = error[0].cast<std::string>();
        out.m_evalue = error[1].cast<std::string>();

        py::list tb = error[2];

        std::transform(
            tb.begin(), tb.end(), std::back_inserter(out.m_traceback),
            [](py::handle obj) -> std::string { return obj.cast<std::string>(); }
        );

        return out;
    }

    xerror extract_already_set_error(py::error_already_set& error)
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

            if (py_tb.ptr() != nullptr && !py_tb.is_none())
            {
                for (py::handle py_frame : py::module::import("traceback").attr("extract_tb")(py_tb))
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

    /********************
     * traceback module *
     ********************/

    py::module get_traceback_module_impl()
    {
        py::module traceback_module = create_module("traceback");

        traceback_module.def("get_filename",
            get_filename,
            py::arg("raw_code")
        );

        traceback_module.def("register_filename_mapping",
            register_filename_mapping,
            py::arg("filename"),
            py::arg("execution_count")
        );

        return traceback_module;
    }

    py::module get_traceback_module()
    {
        static py::module traceback_module = get_traceback_module_impl();
        return traceback_module;
    }
}
