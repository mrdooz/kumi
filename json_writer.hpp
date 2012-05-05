#pragma once

class JsonValue {
  friend class JsonObject;
  friend class JsonArray;
public:
  typedef std::shared_ptr<JsonValue> JsonValuePtr;

  virtual ~JsonValue();

  static JsonValuePtr create_string(const char *str);
  static JsonValuePtr create_number(double value);
  static JsonValuePtr create_int(int value);
  static JsonValuePtr create_bool(bool value);
  static JsonValuePtr create_array();
  static JsonValuePtr create_object();

  virtual bool add_value(JsonValuePtr value);
  virtual bool add_key_value(const std::string &key, JsonValuePtr value);

  std::string print(int indent_size);

protected:
  enum JsonType {
    JS_NULL = 0,
    JS_STRING,
    JS_NUMBER,
    JS_INT,
    JS_OBJECT,
    JS_ARRAY,
    JS_BOOL
  };

  JsonValue(JsonType type);

  virtual void print_inner(int indent_size, std::string *indent_string, std::string *res);

  JsonType _type;
  union {
    const char *_string;
    int _int;
    double _number;
    bool _bool;
  };
};

class JsonObject : public JsonValue {
  friend class JsonValue;
public:
  virtual bool add_key_value(const std::string &key, JsonValuePtr value);

protected:
  JsonObject();

  virtual void print_inner(int indent_size, std::string *indent_string, std::string *res);
  std::map<std::string, JsonValuePtr> _key_value;
};

class JsonArray : public JsonValue {
  friend class JsonValue;
public:
  virtual bool add_value(JsonValuePtr value);

private:
  JsonArray();

  virtual void print_inner(int indent_size, std::string *indent_string, std::string *res);
  std::vector<JsonValuePtr> _value;
};

