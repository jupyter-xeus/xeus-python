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
#include <iostream>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

#include "xdisplay.hpp"
#include "xmatplotlib.hpp"
#include "xutils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    nl::json mime_bundle_repr(const py::object& obj, const py::object& include = py::none(), const py::object& exclude = py::none())
    {
        py::module py_json = py::module::import("json");
        py::module builtins = py::module::import(XPYT_BUILTINS);
        nl::json pub_data;

        // Ugly Matplotlib special case. Matplotlib should implement _repr_mimebundle_ itself
        // try
        // {
            py::module figure = py::module::import("matplotlib.figure");
            py::module backend_inline = get_matplotlib_inline_module();

            if (py::isinstance(obj, figure.attr("Figure")))
            {
                // TODO Support more image types? Reading the current type from the config
                pub_data["image/png"] = backend_inline.attr("print_figure")(obj, "png");
            }
        // }
        // catch (py::error_already_set&)
        // {
            // Ignore any error
        // }

        if (hasattr(obj, "_repr_mimebundle_"))
        {
            pub_data = obj.attr("_repr_mimebundle_")(include, exclude);
        }
        else
        {
            if (hasattr(obj, "_repr_html_"))
            {
                pub_data["text/html"] = py::str(obj.attr("_repr_html_")());
            }
            if (hasattr(obj, "_repr_json_"))
            {
                pub_data["application/json"] = py::str(obj.attr("_repr_json_")());
            }
            if (hasattr(obj, "_repr_jpeg_"))
            {
                pub_data["image/jpeg"] = py::str(obj.attr("_repr_jpeg_")());
            }
            if (hasattr(obj, "_repr_png_"))
            {
                pub_data["image/png"] = py::str(obj.attr("_repr_png_")());
            }
            if (hasattr(obj, "_repr_svg_"))
            {
                pub_data["image/svg+xml"] = py::str(obj.attr("_repr_svg_")());
            }
            if (hasattr(obj, "_repr_latex_"))
            {
                pub_data["text/latex"] = py::str(obj.attr("_repr_latex_")());
            }

            pub_data["text/plain"] = py::str(builtins.attr("repr")(obj));
        }

        return pub_data;
    }

    /****************************
     * xdisplayhook declaration *
     ****************************/

    class xdisplayhook
    {
    public:

        xdisplayhook();
        virtual ~xdisplayhook();

        void set_execution_count(int execution_count);
        void operator()(const py::object& obj, bool raw) const;

    private:

        int m_execution_count;
    };

    /*******************************
     * xdisplayhook implementation *
     *******************************/

    xdisplayhook::xdisplayhook()
        : m_execution_count(0)
    {
    }

    xdisplayhook::~xdisplayhook()
    {
    }

    void xdisplayhook::set_execution_count(int execution_count)
    {
        m_execution_count = execution_count;
    }

    void xdisplayhook::operator()(const py::object& obj, bool raw = false) const
    {
        auto& interp = xeus::get_interpreter();

        if (!obj.is_none())
        {
            if (hasattr(obj, "_ipython_display_"))
            {
                obj.attr("_ipython_display_")();
                return;
            }

            nl::json pub_data;
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                pub_data = mime_bundle_repr(obj);
            }

            interp.publish_execution_result(
                m_execution_count,
                std::move(pub_data),
                nl::json::object()
            );
        }
    }

    /***************************
     * xdisplay implementation *
     ***************************/

    void xdisplay(const py::object& obj, const py::object& include, const py::object& exclude,
                  const py::object& metadata, const py::object& transient, const py::object& display_id,
                  bool update, bool raw)
    {
        auto& interp = xeus::get_interpreter();

        if (!obj.is_none())
        {
            if (hasattr(obj, "_ipython_display_"))
            {
                obj.attr("_ipython_display_")();
                return;
            }

            nl::json pub_data = raw ? nl::json(obj) : mime_bundle_repr(obj, include, exclude);
            nl::json cpp_transient = transient.is_none() ? nl::json::object() : nl::json(transient);
            nl::json cpp_metadata = metadata.is_none() ? nl::json::object() : nl::json(metadata);

            if (!display_id.is_none())
            {
                cpp_transient["display_id"] = display_id;
            }

            if (update)
            {
                interp.update_display_data(
                    std::move(pub_data), std::move(cpp_metadata), std::move(cpp_transient)
                );
            }
            else
            {
                interp.display_data(
                    std::move(pub_data), std::move(cpp_metadata), std::move(cpp_transient)
                );
            }
        }
    }

    /*************************
     * xclear implementation *
     *************************/

    void xclear(bool wait = false)
    {
        auto& interp = xeus::get_interpreter();
        interp.clear_output(wait);
    }

    /******************
     * display module *
     ******************/

    py::module get_display_module_impl()
    {
        py::module display_module("display");

        py::class_<xdisplayhook>(display_module, "DisplayHook")
            .def(py::init<>())
            .def("set_execution_count", &xdisplayhook::set_execution_count)
            .def("__call__", &xdisplayhook::operator(), py::arg("obj"), py::arg("raw") = false);

        display_module.def("display",
              xdisplay,
              py::arg("obj"),
              py::arg("include") = py::none(),
              py::arg("exclude") = py::none(),
              py::arg("metadata") = py::none(),
              py::arg("transient") = py::none(),
              py::arg("display_id") = py::none(),
              py::arg("update") = false,
              py::arg("raw") = false);

        display_module.def("update_display",
              xdisplay,
              py::arg("obj"),
              py::arg("include") = py::none(),
              py::arg("exclude") = py::none(),
              py::arg("metadata") = py::none(),
              py::arg("transient") = py::none(),
              py::arg("display_id") = py::none(),
              py::arg("update") = true,
              py::arg("raw") = false);

        display_module.def("clear_output",
              xclear,
              py::arg("wait") = false);

        return display_module;
    }

    py::module get_display_module()
    {
        static py::module display_module = get_display_module_impl();
        return display_module;
    }
}
