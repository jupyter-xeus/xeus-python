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
#include <cmath>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus/xguid.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/stl.h"

#include "xdisplay.hpp"
#include "xutils.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    bool should_include(const std::string& mimetype, const std::vector<std::string>& include)
    {
        return include.size() == 0 || std::find(include.cbegin(), include.cend(), mimetype) == include.end();
    }

    bool should_exclude(const std::string& mimetype, const std::vector<std::string>& exclude)
    {
        return exclude.size() != 0 && std::find(exclude.cbegin(), exclude.cend(), mimetype) != exclude.end();
    }

    void compute_repr(
        const py::object& obj, const std::string& repr_method, const std::string& mimetype,
        const std::vector<std::string>& include, const std::vector<std::string>& exclude,
        py::dict& pub_data, py::dict& pub_metadata)
    {
        if (hasattr(obj, repr_method.c_str()) && should_include(mimetype, include) && !should_exclude(mimetype, exclude))
        {
            const py::object& repr = obj.attr(repr_method.c_str())();

            if (!repr.is_none())
            {
                if (py::isinstance<py::tuple>(repr))
                {
                    py::tuple repr_tuple = repr;

                    pub_data[mimetype.c_str()] = repr_tuple[0];
                    pub_metadata[mimetype.c_str()] = repr_tuple[1];
                }
                else
                {
                    pub_data[mimetype.c_str()] = repr;
                }
            }
        }
    }

    py::tuple mime_bundle_repr(const py::object& obj, const std::vector<std::string>& include = {}, const std::vector<std::string>& exclude = {})
    {
        py::module py_json = py::module::import("json");
        py::module builtins = py::module::import(XPYT_BUILTINS);
        py::dict pub_data;
        py::dict pub_metadata;

        if (hasattr(obj, "_repr_mimebundle_"))
        {
            pub_data = obj.attr("_repr_mimebundle_")(include, exclude);
        }
        else
        {
            compute_repr(obj, "_repr_html_", "text/html", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_markdown_", "text/markdown", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_svg_", "image/svg+xml", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_png_", "image/png", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_jpeg_", "image/jpeg", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_latex_", "text/latex", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_json_", "application/json", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_javascript_", "application/javascript", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_pdf_", "application/pdf", include, exclude, pub_data, pub_metadata);
        }

        pub_data["text/plain"] = py::str(builtins.attr("repr")(obj));

        return py::make_tuple(pub_data, pub_metadata);
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

            py::object pub_data;
            py::object pub_metadata;
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                const py::tuple& repr = mime_bundle_repr(obj);
                pub_data = repr[0];
                pub_metadata = repr[1];
            }

            interp.publish_execution_result(m_execution_count, pub_data, pub_metadata);
        }
    }

    /***************************
     * xdisplay implementation *
     ***************************/

    void xdisplay(const py::object& obj,
                  const std::vector<std::string>& include, const std::vector<std::string>& exclude,
                  const py::dict& metadata, const py::object& transient, const py::object& display_id,
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

            py::object pub_data;
            py::object pub_metadata = py::dict();
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                const py::tuple& repr = mime_bundle_repr(obj, include, exclude);
                pub_data = repr[0];
                pub_metadata = repr[1];
            }
            pub_metadata.attr("update")(metadata);

            nl::json cpp_transient = transient.is_none() ? nl::json::object() : nl::json(transient);

            if (!display_id.is_none())
            {
                cpp_transient["display_id"] = display_id;
            }

            if (update)
            {
                interp.update_display_data(pub_data, pub_metadata, std::move(cpp_transient));
            }
            else
            {
                interp.display_data(pub_data, pub_metadata, std::move(cpp_transient));
            }
        }
    }

    void xpublish_display_data(const py::object& data, const py::object& metadata, const py::str& /*source*/, const py::object& transient)
    {
        auto& interp = xeus::get_interpreter();

        interp.display_data(data, metadata, transient);
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
              py::arg("include") = py::list(),
              py::arg("exclude") = py::list(),
              py::arg("metadata") = py::dict(),
              py::arg("transient") = py::none(),
              py::arg("display_id") = py::none(),
              py::arg("update") = false,
              py::arg("raw") = false);

        display_module.def("update_display",
              xdisplay,
              py::arg("obj"),
              py::arg("include") = py::list(),
              py::arg("exclude") = py::list(),
              py::arg("metadata") = py::dict(),
              py::arg("transient") = py::none(),
              py::arg("display_id") = py::none(),
              py::arg("update") = true,
              py::arg("raw") = false);

        display_module.def("publish_display_data",
            xpublish_display_data,
            py::arg("data"),
            py::arg("metadata") = py::dict(),
            py::arg("source") = py::str(),
            py::arg("transient") = py::dict());

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

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
