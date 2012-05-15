#pragma once

#include "property.hpp"
#include "property_manager.hpp"

struct Material {

  struct Property {

    Property(const std::string &name, float value) : name(name), type(PropertyType::kFloat) { this->value.x = value; }
    Property(const std::string &name, const XMFLOAT4 &value) : name(name), type(PropertyType::kFloat4), value(value) {}

    std::string name;
    PropertyType::Enum type;
    XMFLOAT4 value;
    PropertyManager::PropertyId id;
  };

  Material(const std::string &name) : name(name), id(nullptr) {}

  template<typename T>
  void add_property(const std::string &name, const T &value) {
    properties.push_back(Property(name, value));
    auto prop_id = PROPERTY_MANAGER.get_or_create<T>(name.c_str(), id);
    properties.back().id = prop_id;
    PROPERTY_MANAGER.set_property(prop_id, value);
  }
  std::string name;
  std::vector<Property> properties;
  PropertyManager::Token id;
};
