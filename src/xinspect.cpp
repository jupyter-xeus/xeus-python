/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>
#include <vector>

#include "xutils.hpp"
#include "xinspect.hpp"

#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace pybind11::literals;

namespace xpyt
{
    py::object static_inspect(const std::string& code, int cursor_pos)
    {
        py::module jedi = py::module::import("jedi");

        py::str py_code = code.substr(0, cursor_pos + 1);

        py::int_ line = 1;
        py::int_ column = 0;
        if (py::len(py_code) != 0)
        {
            py::list lines = py_code.attr("splitlines")();
            line = py::len(lines);
            column = py::len(lines[py::len(lines) - 1]);
        }

        return jedi.attr("Interpreter")(py_code, py::make_tuple(py::globals()), "line"_a = line, "column"_a = column);
    }

    py::object static_inspect(const std::string& code)
    {
        py::module jedi = py::module::import("jedi");
        return jedi.attr("Interpreter")(code, py::make_tuple(py::globals()));
    }

    std::string formatted_docstring_impl(py::object inter)
    {
        py::object definition = py::none();

        // If it's a function call
        py::list call_sig = inter.attr("call_signatures")();
        py::list definitions = inter.attr("goto_definitions")();
        if (py::len(call_sig) != 0)
        {
            definition = call_sig[0];
        }
        else if (py::len(definitions) != 0)
        {
            definition = definitions[0];
        }
        else
        {
            return "";
        }

        auto name = definition.attr("name").cast<std::string>();
        auto docstring = definition.attr("docstring")().cast<std::string>();
        auto type = definition.attr("type").cast<std::string>();

        std::vector<std::string> params;
        // This code will throw if there is no params
        try
        {
            py::list py_params = definition.attr("params");
            for (py::handle param : py_params)
            {
                // keyword arguments?? default value??
                params.push_back(param.attr("name").cast<std::string>());
            }
        }
        catch (...)
        {
        }

        std::string result;
        if (type.compare("function") == 0)
        {
            result.append(red_text("Name:  ") + name + "(");
            for (auto it = params.cbegin(); it < params.cend() - 1; it++)
            {
                result.append(*it + ", ");
            }
            result.append(params.at(params.size() - 1) + ")");
        }
        else
        {
            result.append(red_text("Name:  ") + name);
        }

        result.append(
            red_text("  Type:  ") + type + "\n" +
            red_text("Docstring:\n") + docstring
        );

        return result;
    }

    std::string formatted_docstring(const std::string& code, int cursor_pos)
    {
        py::object inter = static_inspect(code, cursor_pos);
        return formatted_docstring_impl(inter);
    }

    std::string formatted_docstring(const std::string& code)
    {
        py::object inter = static_inspect(code);
        return formatted_docstring_impl(inter);
    }
}
