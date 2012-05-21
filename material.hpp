#pragma once

#include "property.hpp"
#include "property_manager.hpp"

struct Material {

  struct Property {

    Property(const std::string &name, PropertyType::Enum type, float value) 
      : name(name), type(type) { this->value.x = value; }

    Property(const std::string &name, PropertyType::Enum type, const XMFLOAT4 &value) 
      : name(name), type(type), value(value) {}

    std::string name;
    std::string filename;
    PropertyType::Enum type;
    XMFLOAT4 value;
    PropertyManager::PropertyId id;
  };

  Material(const std::string &name) : name(name) {}

  template<typename T>
  void add_property(const std::string &name, PropertyType::Enum type, const T &value) {
    properties.push_back(Property(name, type, value));
    auto prop_id = PROPERTY_MANAGER.get_or_create<T>(name.c_str(), this);
    properties.back().id = prop_id;
    PROPERTY_MANAGER.set_property(prop_id, value);
  }
  std::string name;
  std::vector<Property> properties;
};
