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
## TODO Uncomment the following when IPython is released with https://github.com/ipython/ipython/pull/12809 and remove the rest
# from IPython.core.compilerop import CachingCompiler

# class XCachingCompiler(CachingCompiler):
#     def __init__(self, *args, **kwargs):
#         super(XCachingCompiler, self).__init__(*args, **kwargs)
#
#         self.filename_mapper = None
#
#     def get_code_name(self, raw_code, code, number):
#         filename = get_filename(raw_code)
#
#         if self.filename_mapper is not None:
#             self.filename_mapper(filename, number)
#
#         return filename

import __future__
from ast import PyCF_ONLY_AST
import codeop
import functools
import hashlib
import linecache
import operator
import time
from contextlib import contextmanager

PyCF_MASK = functools.reduce(operator.or_,
                             (getattr(__future__, fname).compiler_flag
                              for fname in __future__.all_feature_names))


class CachingCompiler(codeop.Compile):
    def __init__(self):
        codeop.Compile.__init__(self)

        if not hasattr(linecache, '_ipython_cache'):
            linecache._ipython_cache = {}
        if not hasattr(linecache, '_checkcache_ori'):
            linecache._checkcache_ori = linecache.checkcache

        linecache.checkcache = check_linecache_ipython

        self.filename_mapper = None


    def ast_parse(self, source, filename='<unknown>', symbol='exec'):
        return compile(source, filename, symbol, self.flags | PyCF_ONLY_AST, 1)

    def reset_compiler_flags(self):
        self.flags = codeop.PyCF_DONT_IMPLY_DEDENT

    @property
    def compiler_flags(self):
        return self.flags

    def cache(self, code, number=0, raw_code=None):
        if raw_code is None:
            raw_code = code

        name = get_filename(raw_code)
        entry = (len(code), time.time(),
                 [line+'\n' for line in code.splitlines()], name)
        linecache.cache[name] = entry
        linecache._ipython_cache[name] = entry

        if self.filename_mapper is not None:
            self.filename_mapper(name, number)

        return name

    @contextmanager
    def extra_flags(self, flags):
        turn_on_bits = ~self.flags & flags

        self.flags = self.flags | flags
        try:
            yield
        finally:
            self.flags &= ~turn_on_bits


def check_linecache_ipython(*args):
    linecache._checkcache_ori(*args)
    linecache.cache.update(linecache._ipython_cache)
         )"), compiler_module.attr("__dict__"));

        return compiler_module;
    }

    py::module get_compiler_module()
    {
        static py::module compiler_module = get_compiler_module_impl();
        return compiler_module;
    }
}
