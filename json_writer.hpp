#pragma once

class JsonValue {
  friend class JsonWriter;
public:
  typedef std::shared_ptr<JsonValue> JsonValuePtr;

  virtual ~JsonValue();

  static JsonValuePtr create_string(const char *str);
  static JsonValuePtr create_string(const std::string &str);
  static JsonValuePtr create_number(double value);
  static JsonValuePtr create_int(int value);
  static JsonValuePtr create_bool(bool value);
  static JsonValuePtr create_array();
  static JsonValuePtr create_object();

  virtual bool add_value(JsonValuePtr value);
  virtual bool add_key_value(const std::string &key, JsonValuePtr value);

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
  friend class JsonWriter;
public:
  virtual bool add_key_value(const std::string &key, JsonValuePtr value);

protected:
  JsonObject();
  std::map<std::string, JsonValuePtr> _key_value;
};

class JsonArray : public JsonValue {
  friend class JsonValue;
  friend class JsonWriter;
public:
  virtual bool add_value(JsonValuePtr value);

private:
  JsonArray();
  std::vector<JsonValuePtr> _value;
};

class JsonWriter {
public:
  std::string print(JsonValue::JsonValuePtr root);
private:
  void print_dispatch(const JsonValue *obj, int indent_level, std::string *res);
  void print_inner(const JsonValue *obj, int indent_level, std::string *res);
  void print_inner(const JsonArray *obj, int indent_level, std::string *res);
  void print_inner(const JsonObject *obj, int indent_level, std::string *res);
};
