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

  virtual bool add_value(JsonValuePtr value) { assert(!"Not an array"); return false; }
  virtual bool add_key_value(const std::string &key, JsonValuePtr value) { assert(!"Not an object"); return false; }
  virtual bool add_key_value(const std::string &key, int value) { assert(!"Not an object"); return false; }
  virtual bool add_key_value(const std::string &key, uint32 value)  { assert(!"Not an object"); return false; }
  virtual bool add_key_value(const std::string &key, double value) { assert(!"Not an object"); return false; }
  virtual bool add_key_value(const std::string &key, const std::string &value) { assert(!"Not an object"); return false; }
  virtual bool add_key_value(const std::string &key, bool value) { assert(!"Not an object"); return false; }

  int to_int() const { assert(_type == JS_INT); return _int; }
  double to_number() const { assert(_type == JS_NUMBER || _type == JS_INT); return _type == JS_NUMBER ? _number : _int; }
  bool to_bool() const { assert(_type == JS_BOOL); return _bool; }
  std::string to_string() const { assert(_type == JS_STRING); return _string; }

  virtual JsonValuePtr operator[](int idx) const { assert(!"Not an array"); return _empty_value; }
  virtual JsonValuePtr get(int idx) const { return (*this)[idx]; }
  virtual int count() const { assert(!"Not an array"); return -1; }

  virtual JsonValuePtr operator[](const std::string &key) const { assert(!"Not an object"); return _empty_value; }
  virtual JsonValuePtr get(const std::string &key) const { return (*this)[key]; }
  virtual bool has_key(const std::string &key) const { assert(!"Not an object"); return false; }

  static JsonValuePtr emptyValue();

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
  virtual bool add_key_value(const std::string &key, JsonValuePtr value) override;
  virtual bool add_key_value(const std::string &key, int value) override;
  virtual bool add_key_value(const std::string &key, uint32 value) override;
  virtual bool add_key_value(const std::string &key, double value) override;
  virtual bool add_key_value(const std::string &key, const std::string &value) override;
  virtual bool add_key_value(const std::string &key, bool value) override;

  virtual JsonValuePtr operator[](const std::string &key) const override;
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
  virtual JsonValuePtr operator[](int idx) const override;
  virtual int count() const override { return (int)_value.size(); }

private:
  JsonArray();
  std::vector<JsonValuePtr> _value;
};

std::string print_json(JsonValue::JsonValuePtr root);
JsonValue::JsonValuePtr parse_json(const char *start, const char *end);
