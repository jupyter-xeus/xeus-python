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

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/stl.h"

#include "xeus-python/xutils.hpp"

#include "xdisplay.hpp"
#include "xinternal_utils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{
    /****************************************
     * xpublish_display_data implementation *
     ****************************************/

    void xpublish_display_data(const py::object& data, const py::object& metadata, const py::object& transient, bool update)
    {
        auto& interp = xeus::get_interpreter();

        if (update)
        {
            interp.update_display_data(data, metadata, transient);
        }
        else
        {
            interp.display_data(data, metadata, transient);
        }
    }

    /********************************************
     * xpublish_execution_result implementation *
     ********************************************/

    void xpublish_execution_result(const py::int_& execution_count, const py::object& data, const py::object& metadata)
    {
        auto& interp = xeus::get_interpreter();

        nl::json cpp_data = data;
        if (cpp_data.size() != 0)
        {
            interp.publish_execution_result(execution_count, std::move(cpp_data), metadata);
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
        py::module display_module = create_module("display");

        display_module.def("publish_display_data",
            xpublish_display_data,
            py::arg("data"),
            py::arg("metadata"),
            py::arg("transient"),
            py::arg("update")
        );

        display_module.def("publish_execution_result",
            xpublish_execution_result,
            py::arg("execution_count"),
            py::arg("data"),
            py::arg("metadata")
        );

        display_module.def("clear_output",
            xclear,
            py::arg("wait") = false
        );

        exec(py::str(R"(
import sys

from IPython.core.displaypub import DisplayPublisher
from IPython.core.displayhook import DisplayHook


class XDisplayPublisher(DisplayPublisher):
    def publish(self, data, metadata=None, source=None, *, transient=None, update=False, **kwargs) -> None:
        publish_display_data(data, metadata, transient, update)

    def clear_output(self, wait=False):
        clear_output(wait)


class XDisplayHook(DisplayHook):
    def start_displayhook(self):
        self.data = {}
        self.metadata = {}

    def write_output_prompt(self):
        pass

    def write_format_data(self, format_dict, md_dict=None):
        self.data = format_dict
        self.metadata = md_dict

    def finish_displayhook(self):
        sys.stdout.flush()
        sys.stderr.flush()

        publish_execution_result(self.prompt_count, self.data, self.metadata)

        self.data = {}
        self.metadata = {}
        )"), display_module.attr("__dict__"));

        return display_module;
    }

    py::module get_display_module()
    {
        static py::module display_module = get_display_module_impl();
        return display_module;
    }
}
