#pragma once

#include "property.hpp"

struct MaterialProperty {

  MaterialProperty(const std::string &name, int value) : name(name), type(PropertyType::kInt), _int(value) {}
  MaterialProperty(const std::string &name, float value) : name(name), type(PropertyType::kFloat) { _float[0] = value; }
  MaterialProperty(const std::string &name, const XMFLOAT3 &value) : name(name), type(PropertyType::kFloat3) 
  { _float[0] = value.x; _float[1] = value.z; _float[2] = value.y; _float[3] = 0; }
  MaterialProperty(const std::string &name, const XMFLOAT4 &value) : name(name), type(PropertyType::kFloat4) 
  { _float[0] = value.x; _float[1] = value.z; _float[2] = value.y; _float[3] = value.w; }

  std::string name;
  PropertyType::Enum type;
  union {
    int _int;
    float _float[4];
  };
};

struct Material {
  Material(const std::string &name) : name(name), id(~0) {}
  std::string name;
  std::vector<MaterialProperty> properties;
  uint16 id;  // set by the PropertyManager
};
