#pragma once

#include "property.hpp"
#include "property_manager.hpp"

class Material {
public:
  struct Property {

    Property(const std::string &name, PropertyType::Enum type, float value, PropertyManager::PropertyId id)
      : name(name), type(type), id(id) { this->value.x = value; }

    Property(const std::string &name, PropertyType::Enum type, const XMFLOAT4 &value, PropertyManager::PropertyId id)
      : name(name), type(type), value(value), id(id) {}

    std::string name;
    std::string filename;
    PropertyType::Enum type;
    XMFLOAT4 value;
    PropertyManager::PropertyId id;
  };

  Material(const std::string &name);
  ~Material();

  template<typename T>
  void add_property(const std::string &name, PropertyType::Enum type, const T &value) {
    auto id = PROPERTY_MANAGER.get_or_create<T>(name.c_str(), this);
    Property *prop = new Property(name, type, value, id);
    _properties.push_back(prop);
    _properties_by_name[name] = prop;
    PROPERTY_MANAGER.set_property(id, value);
  }

  const std::string &name() const { return _name; }
  const std::vector<Property *> &properties() const { return _properties; }

  Property *property_by_name(const std::string &name);

private:
  std::string _name;
  std::vector<Property *> _properties;
  std::map<std::string, Property *> _properties_by_name;
};
