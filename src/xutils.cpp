/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cmath>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xcomm.hpp"
#include "xeus/xsystem.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/eval.h"

#include "xtl/xhash.hpp"

#include "xeus-python/xutils.hpp"

#ifdef WIN32
#include "Windows.h"
#endif

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    bool is_pyobject_true(const py::object& obj)
    {
        return PyObject_IsTrue(obj.ptr());
    }

    bool holding_gil()
    {
        return PyGILState_Check();
    }

    void exec(const py::object& code, const py::object& scope)
    {
        py::exec("exec(_code_, _scope_, _scope_)", py::globals(), py::dict(py::arg("_code_") = code, py::arg("_scope_") = scope));
    }

    py::object eval(const py::object& code, const py::object& scope)
    {
        return py::eval(code, scope);
    }
}
