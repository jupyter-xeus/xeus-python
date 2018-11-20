/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>

#include "xeus/xinterpreter.hpp"
#include "xeus/xjson.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"

#include "xdisplay.hpp"

namespace py = pybind11;

namespace xpyt
{
    xdisplayhook::xdisplayhook() : m_execution_count(0)
    {
    }

    xdisplayhook::~xdisplayhook()
    {
    }

    void xdisplayhook::set_execution_count(int execution_count)
    {
        m_execution_count = execution_count;
    }

    void xdisplayhook::operator()(py::object obj)
    {
        auto& interp = xeus::get_interpreter();

        if (!obj.is_none())
        {
            if (hasattr(obj, "_ipython_display_"))
            {
                interp.publish_stream("stderr", "_ipython_display_ is not supported");
            }

            interp.publish_execution_result(
                m_execution_count,
                std::move(display_pub_data(obj)),
                xeus::xjson::object()
            );
        }
    }

    xeus::xjson display_pub_data(py::object obj)
    {
        py::module py_json = py::module::import("json");
        xeus::xjson pub_data;

        if (hasattr(obj, "_repr_mimebundle_"))
        {
            pub_data = xeus::xjson::parse(static_cast<std::string>(
                py::str(py_json.attr("dumps")(obj.attr("_repr_mimebundle_")()))
            ));
        }
        else
        {
            if (hasattr(obj, "_repr_html_"))
            {
                pub_data["text/html"] = static_cast<std::string>(
                    py::str(obj.attr("_repr_html_")())
                );
            }
            if (hasattr(obj, "_repr_json_"))
            {
                pub_data["application/json"] = static_cast<std::string>(
                    py::str(obj.attr("_repr_json_")())
                );
            }
            if (hasattr(obj, "_repr_jpeg_"))
            {
                pub_data["image/jpeg"] = static_cast<std::string>(
                    py::str(obj.attr("_repr_jpeg_")())
                );
            }
            if (hasattr(obj, "_repr_png_"))
            {
                pub_data["image/png"] = static_cast<std::string>(
                    py::str(obj.attr("_repr_png_")())
                );
            }
            if (hasattr(obj, "_repr_svg_"))
            {
                pub_data["image/svg+xml"] = static_cast<std::string>(
                    py::str(obj.attr("_repr_svg_")())
                );
            }
            if (hasattr(obj, "_repr_latex_"))
            {
                pub_data["text/latex"] = static_cast<std::string>(
                    py::str(obj.attr("_repr_latex_")())
                );
            }

            pub_data["text/plain"] = static_cast<std::string>(
                py::str(obj.attr("__repr__")())
            );
        }

        return pub_data;
    }

    PYBIND11_EMBEDDED_MODULE(xeus_python_display, m) {
        py::class_<xdisplayhook>(m, "XPythonDisplay")
            .def(py::init<>())
            .def("set_execution_count", &xdisplayhook::set_execution_count)
            .def("__call__", &xdisplayhook::operator());
    }
}
