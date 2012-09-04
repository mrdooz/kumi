#include "stdafx.h"
#include "shader.hpp"
#include "path_utils.hpp"
#include "utils.hpp"
#include "graphics.hpp"

using namespace std;

Shader::Shader(ShaderType::Enum type) 
  : _type(type)
{
}

bool Shader::on_loaded() {
  _mesh_cbuffer.init();
  _material_cbuffer.init();
  _system_cbuffer.init();
  _instance_cbuffer.init();
  return true;
}

void Shader::set_cbuffer(PropertySource::Enum src, const CBuffer &cbuffer) {
  switch (src) {
    case PropertySource::kMesh: _mesh_cbuffer = cbuffer; break;
    case PropertySource::kMaterial: _material_cbuffer = cbuffer; break;
    case PropertySource::kSystem: _system_cbuffer = cbuffer; break;
    case PropertySource::kInstance: _instance_cbuffer = cbuffer; break;
  }
}

void Shader::set_cbuffer_slot(PropertySource::Enum src, int slot) {
  if (src == PropertySource::kUnknown)
    return;

  if (slot + 1 > (int)_cbuffers.size())
    _cbuffers.resize(slot + 1);
  switch (src) {
    case PropertySource::kMesh: _cbuffers[slot] = &_mesh_cbuffer; break;
    case PropertySource::kMaterial: _cbuffers[slot] = &_material_cbuffer; break;
    case PropertySource::kSystem: _cbuffers[slot] = &_system_cbuffer; break;
    case PropertySource::kInstance: _cbuffers[slot] = &_instance_cbuffer; break;
  }

  KASSERT(_cbuffers[slot]->slot == ~0);
  _cbuffers[slot]->slot = slot;
}

GraphicsObjectHandle Shader::find_cbuffer(const char *name) {
  auto it = _cbuffers_by_name.find(name);
  KASSERT(it != _cbuffers_by_name.end());
  return it == _cbuffers_by_name.end() ? GraphicsObjectHandle() : it->second.handle;
}

void Shader::add_named_cbuffer(const char *name, const CBuffer &cbuffer) {
  _cbuffers_by_name[name] = cbuffer;
  _cbuffers_by_idx.push_back(cbuffer);
}

GraphicsObjectHandle Shader::cbuffer_by_index(int idx) {
  KASSERT(idx < (int)_cbuffers_by_idx.size());
  return _cbuffers_by_idx[idx].handle;
}
