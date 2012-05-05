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

string JsonValue::print(int indent_size) {
  string res;
  print_inner(1, &res);

  return res;
}

void JsonValue::print_inner(int indent_level, string *res) {

  switch (_type) {
  case JS_STRING: 
    res->append("\"");
    res->append(_string);
    res->append("\"");
    break;

  case JS_NUMBER: {
    static char buf[2048];
    sprintf(buf, "%f", _number);
    res->append(buf);
    break;
                  }

  case JS_INT:
    static char buf2[32];
    sprintf(buf2, "%d", _int);
    res->append(buf2);
    break;

  case JS_BOOL:
    res->append(_bool ? "true" : "false");
    break;

  default:
    assert(!"invalid type in print_inner");
    break;
  }
}


bool JsonObject::add_key_value(const string &key, JsonValuePtr value) {
  _key_value[key] = value;
  return true;
}

JsonObject::JsonObject() 
  : JsonValue(JS_OBJECT) 
{

}

void JsonObject::print_inner(int indent_level, string *res) {

  res->append("{\n");

  string indent_string(indent_level, '\t');

  auto it = begin(_key_value);
  if (it != end(_key_value)) {
    stringstream str;
    str << indent_string << "\"" << it->first << "\": ";
    res->append(str.str());
    it->second->print_inner(indent_level + 1, res);
    ++it;
  }

  for (; it != end(_key_value); ++it) {
    stringstream str;
    str << ",\n" << indent_string << "\"" << it->first << "\": ";
    res->append(str.str());

    it->second->print_inner(indent_level + 1, res);
  }

  indent_string = string(indent_level - 1, '\t');

  stringstream str;
  str << "\n" << indent_string << "}";
  res->append(str.str());
}



bool JsonArray::add_value(JsonValuePtr value) {
  _value.push_back(value);
  return true;
}

JsonArray::JsonArray() 
  : JsonValue(JS_ARRAY) 

{
}

void JsonArray::print_inner(int indent_level, string *res) {
  string indent_string(indent_level, '\t');

  res->append("[\n");

  auto it = begin(_value);
  if (it != end(_value)) {
    res->append(indent_string);
    (*it)->print_inner(indent_level + 1, res);
    ++it;
  }

  for (; it != end(_value); ++it) {
    stringstream str;
    str << ",\n" << indent_string;
    res->append(str.str());
    (*it)->print_inner(indent_level + 1, res);
  }

  indent_string = string(indent_level - 1, '\t');

  stringstream str;
  str << "\n" << indent_string << "]";
  res->append(str.str());
}

