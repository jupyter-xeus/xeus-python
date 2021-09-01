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
