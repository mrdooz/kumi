#pragma once

#include "property.hpp"
#include "property_manager.hpp"

class Material {
public:
  struct Property {

    Property(const std::string &name, PropertyType::Enum type, float value, PropertyId id)
      : name(name), type(type), id(id) { _float4[0] = value; }

    Property(const std::string &name, PropertyType::Enum type, int value, PropertyId id)
      : name(name), type(type), id(id) { _int = value; }

    Property(const std::string &name, PropertyType::Enum type, const XMFLOAT4 &value, PropertyId id)
      : name(name), type(type), id(id) { memcpy(&_float4, &value, sizeof(value)); }

    std::string name;
    std::string filename;
    PropertyType::Enum type;
    union {
      float _float4[4];
      int _int;
    };
    PropertyId id;
    PropertyId class_id;
  };

  Material(const std::string &name);
  ~Material();

  template<typename T>
  void add_property(const std::string &name, PropertyType::Enum type, const T &value) {
    auto id = PROPERTY_MANAGER.get_or_create_raw(name.c_str(), this, sizeof(T), &value);
    std::string class_name = "Material::" + name;
    auto class_id = PROPERTY_MANAGER.get_or_create_raw(class_name.c_str(), sizeof(int), nullptr);
    Property *prop = new Property(name, type, value, id);
    prop->class_id = class_id;
    _properties.push_back(prop);
    _properties_by_name[name] = prop;
    _properties_by_id[class_id] = prop;
  }

  const std::string &name() const { return _name; }
  const std::vector<Property *> &properties() const { return _properties; }

  Property *property_by_name(const std::string &name);

  void fill_cbuffer(CBuffer *cbuffer);

private:
  std::string _name;
  std::vector<Property *> _properties;
  std::map<std::string, Property *> _properties_by_name;
  std::map<int, Property *> _properties_by_id;
};
