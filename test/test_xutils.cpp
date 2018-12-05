/***************************************************************************
* Copyright (c) 2016, Johan Mabille, Sylvain Corlay, Wolf Vollprecht and   *
* Martin Renou                                                             *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <string>

#include "gtest/gtest.h"

#include "../src/xutils.hpp"

#include "pybind11/embed.h"

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt
{

    TEST(pyobject_tojson, none)
    {
        py::scoped_interpreter guard;
        py::object obj = py::none();
        nl::json j = obj;

        ASSERT_TRUE(j.is_null());
    }

    TEST(pyobject_tojson, bool_)
    {
        py::scoped_interpreter guard;
        py::object obj = py::bool_(false);
        nl::json j = obj;

        ASSERT_TRUE(j.is_boolean());
        ASSERT_FALSE(j.get<bool>());
    }

    TEST(pyobject_tojson, number)
    {
        py::scoped_interpreter guard;
        py::object obj = py::int_(36);
        nl::json j = obj;

        ASSERT_TRUE(j.is_number());
        ASSERT_EQ(j.get<int>(), 36);

        py::object obj2 = py::float_(36.37);
        nl::json j2 = obj2;

        ASSERT_TRUE(j2.is_number());
        ASSERT_EQ(j2.get<double>(), 36.37);
    }

    TEST(pyobject_tojson, string)
    {
        py::scoped_interpreter guard;
        py::object obj = py::str("Hello");
        nl::json j = obj;

        ASSERT_TRUE(j.is_string());
        ASSERT_EQ(j.get<std::string>(), "Hello");
    }

    TEST(pyobject_tojson, list)
    {
        py::scoped_interpreter guard;
        py::object obj = py::list();
        obj.attr("append")(py::int_(36));
        obj.attr("append")(py::str("Hello World"));
        obj.attr("append")(py::bool_(false));
        nl::json j = obj;

        ASSERT_TRUE(j.is_array());
        ASSERT_EQ(j[0].get<int>(), 36);
        ASSERT_EQ(j[1].get<std::string>(), "Hello World");
        ASSERT_EQ(j[2].get<bool>(), false);
    }

    TEST(pyobject_tojson, tuple)
    {
        py::scoped_interpreter guard;
        py::object obj = py::make_tuple(1234, "hello", false);
        nl::json j = obj;

        ASSERT_TRUE(j.is_array());
        ASSERT_EQ(j[0].get<int>(), 1234);
        ASSERT_EQ(j[1].get<std::string>(), "hello");
        ASSERT_EQ(j[2].get<bool>(), false);
    }

    TEST(pyobject_tojson, dict)
    {
        py::scoped_interpreter guard;
        py::object obj = py::dict("number"_a=1234, "hello"_a="world");
        nl::json j = obj;

        ASSERT_TRUE(j.is_object());
        ASSERT_EQ(j["number"].get<int>(), 1234);
        ASSERT_EQ(j["hello"].get<std::string>(), "world");
    }

    TEST(pyobject_tojson, nested)
    {
        py::scoped_interpreter guard;
        py::object obj = py::dict(
            "list"_a=py::make_tuple(1234, "hello", false),
            "dict"_a=py::dict("a"_a=12, "b"_a=13),
            "hello"_a="world",
            "world"_a=py::none()
        );
        nl::json j = obj;

        ASSERT_TRUE(j.is_object());
        ASSERT_EQ(j["list"][0].get<int>(), 1234);
        ASSERT_EQ(j["list"][1].get<std::string>(), "hello");
        ASSERT_EQ(j["list"][2].get<bool>(), false);
        ASSERT_EQ(j["dict"]["a"].get<int>(), 12);
        ASSERT_EQ(j["dict"]["b"].get<int>(), 13);
        ASSERT_EQ(j["world"], nullptr);
    }

    TEST(pyobject_fromjson, none)
    {
        py::scoped_interpreter guard;
        nl::json j = "null"_json;
        py::object obj = j;

        ASSERT_TRUE(obj.is_none());
    }

    TEST(pyobject_fromjson, bool_)
    {
        py::scoped_interpreter guard;
        nl::json j = "false"_json;
        py::object obj = j;

        ASSERT_TRUE(py::isinstance<py::bool_>(obj));
        ASSERT_FALSE(obj.cast<bool>());
    }

    TEST(pyobject_fromjson, number)
    {
        py::scoped_interpreter guard;
        nl::json j = "36"_json;
        py::object obj = j;

        ASSERT_TRUE(py::isinstance<py::int_>(obj));
        ASSERT_EQ(obj.cast<int>(), 36);

        nl::json j2 = "36.2"_json;
        py::object obj2 = j2;

        ASSERT_TRUE(py::isinstance<py::float_>(obj2));
        ASSERT_EQ(obj2.cast<double>(), 36.2);
    }

    TEST(pyobject_fromjson, string)
    {
        py::scoped_interpreter guard;
        nl::json j = "\"Hello World!\""_json;
        py::object obj = j;

        ASSERT_TRUE(py::isinstance<py::str>(obj));
        ASSERT_EQ(obj.cast<std::string>(), "Hello World!");
    }

    TEST(pyobject_fromjson, list)
    {
        py::scoped_interpreter guard;
        nl::json j = "[1234, \"Hello World!\", false]"_json;
        py::object obj = j;

        ASSERT_TRUE(py::isinstance<py::list>(obj));
        ASSERT_EQ(py::list(obj)[0].cast<int>(), 1234);
        ASSERT_EQ(py::list(obj)[1].cast<std::string>(), "Hello World!");
        ASSERT_EQ(py::list(obj)[2].cast<bool>(), false);
    }

    TEST(pyobject_fromjson, dict)
    {
        py::scoped_interpreter guard;
        nl::json j = "{\"a\": 1234, \"b\":\"Hello World!\", \"c\":false}"_json;
        py::object obj = j;

        ASSERT_TRUE(py::isinstance<py::dict>(obj));
        ASSERT_EQ(py::dict(obj)["a"].cast<int>(), 1234);
        ASSERT_EQ(py::dict(obj)["b"].cast<std::string>(), "Hello World!");
        ASSERT_EQ(py::dict(obj)["c"].cast<bool>(), false);
    }

    TEST(pyobject_fromjson, nested)
    {
        py::scoped_interpreter guard;
        nl::json j = R"({
            "baz": ["one", "two", "three"],
            "foo": 1,
            "bar": {"a": 36, "b": false},
            "hey": null
        })"_json;
        py::object obj = j;

        ASSERT_TRUE(py::isinstance<py::dict>(obj));
        ASSERT_TRUE(py::isinstance<py::list>(py::dict(obj)["baz"]));
        ASSERT_TRUE(py::isinstance<py::int_>(py::dict(obj)["foo"]));
        py::list baz = py::dict(obj)["baz"];
        py::int_ foo = py::dict(obj)["foo"];
        py::dict bar = py::dict(obj)["bar"];
        ASSERT_EQ(baz[0].cast<std::string>(), "one");
        ASSERT_EQ(baz[1].cast<std::string>(), "two");
        ASSERT_EQ(baz[2].cast<std::string>(), "three");
        ASSERT_EQ(foo.cast<int>(), 1);
        ASSERT_EQ(bar["a"].cast<int>(), 36);
        ASSERT_FALSE(bar["b"].cast<bool>());
        ASSERT_TRUE(py::dict(obj)["hey"].is_none());
    }

}
