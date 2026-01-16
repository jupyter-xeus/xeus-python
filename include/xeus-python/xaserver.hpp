/***************************************************************************
* Copyright (c) 2024, Isabel Paredes                                       *
* Copyright (c) 2024, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "xeus_python_config.hpp"
#include "xeus/xkernel.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{

    XEUS_PYTHON_API xeus::xkernel::server_builder make_xaserver_factory(py::dict globals);

} // namespace xeus
