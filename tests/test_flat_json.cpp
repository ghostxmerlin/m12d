#include <string>
#include "doctest.h"
#include "proto/flat_json.h"

TEST_CASE("flat_json: normal object") {
  auto o = parse_flat_json(R"( {"id": 7, "do":"cfg", "lm":false, "x":null, "f":-1.5e1} )");
  REQUIRE(o);
  CHECK(std::get<double>(o->at("id")) == 7);
  CHECK(std::get<std::string>(o->at("do")) == "cfg");
  CHECK(std::get<bool>(o->at("lm")) == false);
  CHECK(std::holds_alternative<std::nullptr_t>(o->at("x")));
  CHECK(std::get<double>(o->at("f")) == -15.0);
}

TEST_CASE("flat_json: string escapes") {
  auto o = parse_flat_json(R"({"s":"a\"b\\c\ndé\/"})");
  REQUIRE(o);
  CHECK(std::get<std::string>(o->at("s")) == "a\"b\\c\nd\xC3\xA9/");
}

TEST_CASE("flat_json: empty object") {
  auto o = parse_flat_json("{}");
  REQUIRE(o);
  CHECK(o->empty());
}

TEST_CASE("flat_json: rejects malformed input") {
  CHECK_FALSE(parse_flat_json(R"({"a":1,})"));        // trailing comma
  CHECK_FALSE(parse_flat_json(R"({"a":1)"));          // not closed
  CHECK_FALSE(parse_flat_json(R"({"a":{"b":1}})"));   // nested object
  CHECK_FALSE(parse_flat_json(R"({"a":[1]})"));       // array
  CHECK_FALSE(parse_flat_json(R"({"a":1,"a":2})"));   // duplicate key
  CHECK_FALSE(parse_flat_json(R"({"a":1} x)"));       // trailing garbage
  CHECK_FALSE(parse_flat_json(R"([1,2])"));           // not an object
  CHECK_FALSE(parse_flat_json(R"({"a":01})"));        // leading zero
  CHECK_FALSE(parse_flat_json(R"({"a":tru})"));
  CHECK_FALSE(parse_flat_json(R"({"a":"x\q"})"));     // bad escape
  CHECK_FALSE(parse_flat_json("{\"a\":\"\x01\"}"));  // raw control char
  CHECK_FALSE(parse_flat_json(""));
  CHECK_FALSE(parse_flat_json("hello"));
}

TEST_CASE("flat_json: key limit") {
  std::string s32 = "{", s33 = "{";
  for (int i = 0; i < 33; i++) {
    std::string kv = "\"k" + std::to_string(i) + "\":1";
    if (i < 32) s32 += (i ? "," : "") + kv;
    s33 += (i ? "," : "") + kv;
  }
  s32 += "}";
  s33 += "}";
  CHECK(parse_flat_json(s32));
  CHECK_FALSE(parse_flat_json(s33));
}
