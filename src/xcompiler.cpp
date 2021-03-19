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
#include <utility>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"

#include "xeus-python/xutils.hpp"

#include "xcompiler.hpp"
#include "xinternal_utils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{

    py::str get_filename(const py::str& raw_code)
    {
        return get_cell_tmp_file(raw_code);
    }

    /*******************
     * compiler module *
     *******************/

    py::module get_compiler_module_impl()
    {
        py::module compiler_module = create_module("compiler");

        compiler_module.def("get_filename", get_filename);

        ::xpyt::exec(py::str(R"(
from IPython.core.compilerop import CachingCompiler

class XCachingCompiler(CachingCompiler):
    def __init__(self, *args, **kwargs):
        super(XCachingCompiler, self).__init__(*args, **kwargs)

        self.filename_mapper = None

    def get_code_name(self, raw_code, code, number):
        filename = get_filename(raw_code)

        if self.filename_mapper is not None:
            self.filename_mapper(filename, number)

        return filename
         )"), compiler_module.attr("__dict__"));

        return compiler_module;
    }

    py::module get_compiler_module()
    {
        static py::module compiler_module = get_compiler_module_impl();
        return compiler_module;
    }
}
