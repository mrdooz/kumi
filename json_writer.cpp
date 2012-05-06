#include "stdafx.h"
#include "json_writer.hpp"
#include "utils.hpp"
#include <sstream>

using namespace std;

JsonValue::JsonValue(JsonType type)
  : _type(type)
{
}

JsonValue::~JsonValue() {
  switch (_type) {
  case JS_STRING: SAFE_FREE(_string); break;
  }
}

JsonValue::JsonValuePtr JsonValue::create_string(const char *str) {
  auto js = JsonValuePtr(new JsonValue(JS_STRING));
  js->_string = strdup(str);
  return js;
}

JsonValue::JsonValuePtr JsonValue::create_string(const std::string &str) {
  return create_string(str.c_str());
}

JsonValue::JsonValuePtr JsonValue::create_number(double value) {
  auto js = JsonValuePtr(new JsonValue(JS_NUMBER));
  js->_number = value;
  return js;
}

JsonValue::JsonValuePtr JsonValue::create_int(int value) {
  auto js = JsonValuePtr(new JsonValue(JS_INT));
  js->_int = value;
  return js;
}

JsonValue::JsonValuePtr JsonValue::create_bool(bool value) {
  auto js = JsonValuePtr(new JsonValue(JS_BOOL));
  js->_bool = value;
  return js;
}

JsonValue::JsonValuePtr JsonValue::create_array() {
  return JsonValuePtr(new JsonArray());
}

JsonValue::JsonValuePtr JsonValue::create_object() {
  return JsonValuePtr(new JsonObject());
}

bool JsonValue::add_value(JsonValuePtr value) {
  return false;
}

bool JsonValue::add_key_value(const string &key, JsonValuePtr value) {
  return false;
}

bool JsonObject::add_key_value(const string &key, JsonValuePtr value) {
  _key_value[key] = value;
  return true;
}

JsonObject::JsonObject() 
  : JsonValue(JS_OBJECT) 
{
}

bool JsonArray::add_value(JsonValuePtr value) {
  _value.push_back(value);
  return true;
}

JsonArray::JsonArray() 
  : JsonValue(JS_ARRAY) 

{
}

string JsonWriter::print(JsonValue::JsonValuePtr root) {
  string res;
  print_dispatch(root.get(), 1, &res);
  return res;
}

void JsonWriter::print_inner(const JsonValue *obj, int indent_level, std::string *res) {

  switch (obj->_type) {
  case JsonValue::JS_STRING: 
    res->append("\"");
    res->append(obj->_string);
    res->append("\"");
    break;

  case JsonValue::JS_NUMBER: {
    static char buf[2048];
    sprintf(buf, "%f", obj->_number);
    res->append(buf);
    break;
                  }

  case JsonValue::JS_INT:
    static char buf2[32];
    sprintf(buf2, "%d", obj->_int);
    res->append(buf2);
    break;

  case JsonValue::JS_BOOL:
    res->append(obj->_bool ? "true" : "false");
    break;

  default:
    assert(!"invalid type in print_inner");
    break;
  }
}

void JsonWriter::print_inner(const JsonObject *obj, int indent_level, std::string *res) {

  res->append("{\n");

  string indent_string(indent_level, '\t');

  auto it = begin(obj->_key_value);
  if (it != end(obj->_key_value)) {
    stringstream str;
    str << indent_string << "\"" << it->first << "\": ";
    res->append(str.str());
    print_dispatch(it->second.get(), indent_level + 1, res);
    ++it;
  }

  for (; it != end(obj->_key_value); ++it) {
    stringstream str;
    str << ",\n" << indent_string << "\"" << it->first << "\": ";
    res->append(str.str());

    print_dispatch(it->second.get(), indent_level + 1, res);
  }

  indent_string = string(indent_level - 1, '\t');

  stringstream str;
  str << "\n" << indent_string << "}";
  res->append(str.str());
}

void JsonWriter::print_inner(const JsonArray *obj, int indent_level, std::string *res) {
  string indent_string(indent_level, '\t');

  res->append("[\n");

  auto it = begin(obj->_value);
  if (it != end(obj->_value)) {
    res->append(indent_string);
    print_dispatch(it->get(), indent_level + 1, res);
    ++it;
  }

  for (; it != end(obj->_value); ++it) {
    stringstream str;
    str << ",\n" << indent_string;
    res->append(str.str());
    print_dispatch(it->get(), indent_level + 1, res);
  }

  indent_string = string(indent_level - 1, '\t');

  stringstream str;
  str << "\n" << indent_string << "]";
  res->append(str.str());
}

void JsonWriter::print_dispatch(const JsonValue *obj, int indent_level, std::string *res) {
  // meh, this is kinda cheesy..
  switch (obj->_type) {
    case JsonValue::JS_ARRAY:
      return print_inner(static_cast<const JsonArray *>(obj), indent_level, res);
    case JsonValue::JS_OBJECT:
      return print_inner(static_cast<const JsonObject *>(obj), indent_level, res);
    default:
      return print_inner(obj, indent_level, res);
  }
}
