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

  auto j = begin(_property_list);
  for (auto i = begin(cbuffer->vars); i != end(cbuffer->vars); ++i) {
    // find the correct material id.
    while (j != end(_property_list) && i->id != j->second->class_id)
      ++j;
    if (j == end(_property_list))
      return;
    memcpy(&cbuffer->staging[i->ofs], j->second->_data, i->len);
  }
}

int Material::flags() const {
  return _flags;
}

void Material::add_flag(int flag) {
  _flags |= flag;
}

void Material::fill_resource_views(const ResourceViewArray &views, TextureArray *out) const {
  for (size_t i = 0; i < views.size(); ++i) {
    if (views[i].used && views[i].source == PropertySource::kMaterial) {
      auto it = _properties_by_class_id.find(views[i].class_id);
      if (it != _properties_by_class_id.end()) {
        (*out)[i] = it->second->resource;
      }
    }
  }
}
