#include "stdafx.h"
#include "material.hpp"
#include "utils.hpp"

Material::Material(const std::string &name)
  : _name(name) 
  , _flags(0)
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

void Material::fill_cbuffer(CBuffer *cbuffer) const {
  for (size_t i = 0; i < cbuffer->material_vars.size(); ++i) {
    auto &cur = cbuffer->material_vars[i];
    auto it = _properties_by_id.find(cur.id);
    if (it != _properties_by_id.end()) {
      PROPERTY_MANAGER.get_property_raw(it->second->id, &cbuffer->staging[cur.ofs], cur.len);
    }
  }
}

int Material::flags() const {
  return _flags;
}

void Material::add_flag(int flag) {
  _flags |= flag;
}

void Material::fill_resource_views(const SparseUnknown &props, std::vector<GraphicsObjectHandle> *out) const {
  
  out->resize(props.res.size());

  for (size_t i = 0; i < props.res.size(); ++i) {
    if (props.res[i].source == PropertySource::kMaterial) {
      auto it = _properties_by_id.find(*props.res[i].pid());
      if (it != _properties_by_id.end()) {
        (*out)[i] = it->second->resource;
      }
    }
  }
}
