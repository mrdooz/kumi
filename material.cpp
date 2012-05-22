#include "stdafx.h"
#include "material.hpp"
#include "utils.hpp"

Material::Material(const std::string &name)
  : _name(name) 
{
}

Material::~Material() {
  seq_delete(&_properties);
  _properties_by_name.clear();
}

Material::Property *Material::property_by_name(const std::string &name) {
  auto it = _properties_by_name.find(name);
  return it == end(_properties_by_name) ? nullptr : it->second;
}
