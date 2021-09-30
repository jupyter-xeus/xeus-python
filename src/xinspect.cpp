/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>
#include <vector>

#include "xeus-python/xutils.hpp"
#include "xinspect.hpp"

#include "pybind11/pybind11.h"

#include "xinternal_utils.hpp"

using namespace pybind11::literals;

namespace xpyt
{
    py::object static_inspect(const std::string& code)
    {
        py::module jedi = py::module::import("jedi");
        return jedi.attr("Interpreter")(code, py::make_tuple(py::globals()));
    }

    py::object static_inspect(const std::string& code, int cursor_pos)
    {
        std::string sub_code = code.substr(0, cursor_pos);
        return static_inspect(sub_code);
    }

    py::list get_completions(const std::string& code, int cursor_pos)
    {
        return static_inspect(code, cursor_pos).attr("complete")();
    }

    py::list get_completions(const std::string& code)
    {
        return static_inspect(code).attr("complete")();
    }

    py::list get_completions(const std::string& code);

    std::string formatted_docstring_impl(py::object inter)
    {
        py::object definition = py::none();

        // If it's a function call
        py::list call_sig = inter.attr("get_signatures")();
        py::list definitions = inter.attr("infer")();
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

        std::string result;

        // Retrieving the argument names and default values for keyword arguments if it's a function
        py::list signatures = definition.attr("get_signatures")();
        if (py::len(signatures) != 0)
        {
            py::list py_params = signatures[0].attr("params");
            result.append(red_text("Signature: ") + name + blue_text("("));

            for (py::handle param : py_params)
            {
                py::list param_description = param.attr("to_string")().attr("split")("=");
                std::string param_name = param_description[0].cast<std::string>();

                // The argument is not a kwarg (no default value)
                if (py::len(param_description) == 1)
                {
                    result.append(param_name);
                }
                else
                {
                    std::string default_value = param_description[1].cast<std::string>();
                    result.append(param_name + blue_text("=") + green_text(default_value));
                }

                // If it's not the last element, add a comma.
                if (!param.is(py_params[py::len(py_params) - 1]))
                {
                    result.append(blue_text(", "));
                }
            }

            result.append(blue_text(")"));

            // Remove signature from the docstring
            py::list splitted_docstring = definition.attr("docstring")().attr("split")("\n\n", 1);
            if (py::len(splitted_docstring) == 2)
            {
                docstring = splitted_docstring[1].cast<std::string>();
            }
        }
        else
        {
            result.append(red_text("Name: ") + name);
        }

        result.append(red_text("\nType: ") + type + red_text("\nDocstring: ") + docstring);

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
