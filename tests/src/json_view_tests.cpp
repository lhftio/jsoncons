// Copyright 2020 Daniel Parker
// Distributed under Boost license

#include <jsoncons/json.hpp>
#include <jsoncons/json_view.hpp>
#include <catch/catch.hpp>
#include <sstream>
#include <vector>
#include <utility>
#include <ctime>
#include <map>
#include <iterator>

using namespace jsoncons;

TEST_CASE("json_const_pointer array tests")
{
    json j = json::parse(R"( ["one", "two", "three"] )");

    SECTION("size()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_array());
        CHECK(v.size() == 3);
        CHECK_FALSE(v.empty());
    }
    SECTION("at()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_array());
        REQUIRE_THROWS(v.at(1));
    }
    SECTION("copy")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        CHECK(v.storage() == view_storage_kind::json_const_pointer);

        json_view j2(v);
        CHECK(j2.storage() == view_storage_kind::array_value);
    }
    SECTION("assignment")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        CHECK(v.storage() == view_storage_kind::json_const_pointer);

        json_view<json> j2;
        j2 = v;
        CHECK(j2.storage() == view_storage_kind::array_value);
    }
}

TEST_CASE("json_const_pointer object tests")
{
    json j = json::parse(R"( {"one" : 1, "two" : 2, "three" : 3} )");

    SECTION("size()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_object());
        CHECK(v.size() == 3);
        CHECK_FALSE(v.empty());
    }
    SECTION("at()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_object());
        REQUIRE_THROWS(v.at("two"));
        CHECK(v.contains("two"));
        CHECK(v.count("two") == 1);

        CHECK(v.get_value_or<int>("three", 0) == 3);
        CHECK(v.get_value_or<int>("four", 4) == 4);
    }
}

TEST_CASE("json_const_pointer string tests")
{
    json j = json("Hello World");

    SECTION("is_string()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_string());
        REQUIRE(v.is_string_view());

        CHECK(v.as<std::string>() == j.as<std::string>());
    }
}

TEST_CASE("json_const_pointer byte_string tests")
{
    std::string data = "abcdefghijk";
    json j(byte_string_arg, data);

    SECTION("is_byte_string()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_byte_string());
        REQUIRE(v.is_byte_string_view());
    }
}

TEST_CASE("json_const_pointer bool tests")
{
    json tru(true);
    json fal(false);

    SECTION("true")
    {
        json_view<json> v(json_const_pointer_arg, &tru);
        REQUIRE(v.is_bool());
        CHECK(v.as_bool());
    }
    SECTION("false")
    {
        json_view<json> v(json_const_pointer_arg, &fal);
        REQUIRE(v.is_bool());
        CHECK_FALSE(v.as_bool());
    }
}

TEST_CASE("json_const_pointer int64 tests")
{
    json j(-100);

    SECTION("is_int64()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_int64());
        CHECK(v.as<int64_t>() == -100);
    }
}

TEST_CASE("json_const_pointer uint64 tests")
{
    json j(100);

    SECTION("is_uint64()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_uint64());
        CHECK(v.as<uint64_t>() == 100);
    }
}

TEST_CASE("json_const_pointer half tests")
{
    json j(half_arg, 100);

    SECTION("is_half()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_half());
        CHECK(v.as<uint16_t>() == 100);
    }
}

TEST_CASE("json_const_pointer double tests")
{
    json j(123.456);

    SECTION("is_double()")
    {
        json_view<json> v(json_const_pointer_arg, &j);
        REQUIRE(v.is_double());

        CHECK(v.as_double() == 123.456);
    }
}

namespace {

    void flatten(const json_view<json>& source, 
                 const std::string& identifier, 
                 json_view<json>& result)
    {
        json_view<json> temp(json_array_arg);
        for (auto& item : source.array_range())
        {
            if (item.is_array())
            {
                for (auto& item_of_item : item.array_range())
                {
                    temp.emplace_back(json_const_pointer_arg,std::addressof(item_of_item));
                }
            }
            else
            {
                temp.emplace_back(json_const_pointer_arg, std::addressof(item));
            }
        }
        for (const auto& item : temp.array_range())
        {
            if (!item.is_null())
            {
                const auto& j = item.contains(identifier) ? item.at(identifier) : json::null();
                if (!j.is_null())
                {
                    result.emplace_back(json_const_pointer_arg, std::addressof(j));
                }
            }
        }
    }
}

TEST_CASE("json_const_pointer identifier tests")
{
    json source = json::parse(R"(
    {"reservations": [{
        "instances": [
            {"foo": [{"bar": 1}, {"bar": 2}, {"notbar": 3}, {"bar": 4}]},
            {"foo": [{"bar": 5}, {"bar": 6}, {"notbar": [7]}, {"bar": 8}]},
            {"foo": "bar"},
            {"notfoo": [{"bar": 20}, {"bar": 21}, {"notbar": [7]}, {"bar": 22}]},
            {"bar": [{"baz": [1]}, {"baz": [2]}, {"baz": [3]}, {"baz": [4]}]},
            {"baz": [{"baz": [1, 2]}, {"baz": []}, {"baz": []}, {"baz": [3, 4]}]},
            {"qux": [{"baz": []}, {"baz": [1, 2, 3]}, {"baz": [4]}, {"baz": []}]}
        ],
        "otherkey": {"foo": [{"bar": 1}, {"bar": 2}, {"notbar": 3}, {"bar": 4}]}
      }, {
        "instances": [
            {"a": [{"bar": 1}, {"bar": 2}, {"notbar": 3}, {"bar": 4}]},
            {"b": [{"bar": 5}, {"bar": 6}, {"notbar": [7]}, {"bar": 8}]},
            {"c": "bar"},
            {"notfoo": [{"bar": 23}, {"bar": 24}, {"notbar": [7]}, {"bar": 25}]},
            {"qux": [{"baz": []}, {"baz": [1, 2, 3]}, {"baz": [4]}, {"baz": []}]}
        ],
        "otherkey": {"foo": [{"bar": 1}, {"bar": 2}, {"notbar": 3}, {"bar": 4}]}
      }
    ]}
    )");

    SECTION("test1")
    {
        json target;
        json expected = json::parse("[1,2,4,5,6,8]");

        json_view<json> j1(json_array_arg);
        json_view<json> j2(json_array_arg);
        json_view<json> j3(json_array_arg);
        
        const json_view<json> v1(json_const_pointer_arg, &source.at("reservations"));
        flatten(v1, "instances", j1);

        //const json_view<json> v2(json_const_pointer_arg, &j1);
        //flatten(v2, "foo", j2);

        //const json_view<json> v3(json_const_pointer_arg, &j2);
        //flatten(v3, "bar", j3);

        //target = j3;
        
        //CHECK(target == expected);
    }
}

