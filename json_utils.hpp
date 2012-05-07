#pragma once

class JsonValue {
  friend class JsonWriter;
public:
  typedef std::shared_ptr<JsonValue> JsonValuePtr;

  enum JsonType {
    JS_NULL = 0,
    JS_STRING,
    JS_NUMBER,
    JS_INT,
    JS_OBJECT,
    JS_ARRAY,
    JS_BOOL
  };

  virtual ~JsonValue();

  static JsonValuePtr create_string(const char *str);
  static JsonValuePtr create_string(const std::string &str);
  static JsonValuePtr create_number(double value);
  static JsonValuePtr create_int(int value);
  static JsonValuePtr create_bool(bool value);
  static JsonValuePtr create_array();
  static JsonValuePtr create_object();

  JsonType type() const { return _type; }

  virtual bool add_value(JsonValuePtr value);
  virtual bool add_key_value(const std::string &key, JsonValuePtr value);

  int get_int() const { assert(_type == JS_INT); return _int; }
  double get_number() const { assert(_type == JS_NUMBER); return _number; }
  bool get_bool() const { assert(_type == JS_BOOL); return _bool; }
  std::string get_string() const { assert(_type == JS_STRING); return _string; }

  virtual JsonValuePtr get_at_index(int idx) const { assert(!"Not an array"); return _empty_value; }
  virtual int get_count() const { assert(!"Not an array"); return -1; }

  virtual JsonValuePtr get_by_key(const std::string &key) const { assert(!"Not an object"); return _empty_value; }
  virtual bool has_key(const std::string &key) const { assert(!"Not an object"); return false; }

protected:

  static JsonValuePtr _empty_value;

  JsonValue();
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
  friend void print_inner(const JsonObject *obj, int indent_level, std::string *res);
public:
  virtual bool add_key_value(const std::string &key, JsonValuePtr value);
  virtual JsonValuePtr get_by_key(const std::string &key) const;
  virtual bool has_key(const std::string &key) const;

protected:
  JsonObject();
  std::map<std::string, JsonValuePtr> _key_value;
};

class JsonArray : public JsonValue {
  friend class JsonValue;
  friend class JsonWriter;
  friend void print_inner(const JsonArray *obj, int indent_level, std::string *res);
public:
  virtual bool add_value(JsonValuePtr value);
  virtual JsonValuePtr get_at_index(int idx) const;
  virtual int get_count() const { return (int)_value.size(); }

private:
  JsonArray();
  std::vector<JsonValuePtr> _value;
};

std::string print_json(JsonValue::JsonValuePtr root);
JsonValue::JsonValuePtr parse_json(const char *start, const char *end);
