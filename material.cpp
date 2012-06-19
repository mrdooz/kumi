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

void Material::fill_cbuffer(CBuffer *cbuffer) {
  for (size_t i = 0; i < cbuffer->material_vars.size(); ++i) {
    auto &cur = cbuffer->material_vars[i];
    auto prop = _properties_by_id[cur.id];
    if (prop)
      PROPERTY_MANAGER.get_property_raw(prop->id, &cbuffer->staging[cur.ofs], cur.len);
  }
}
