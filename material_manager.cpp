#include "stdafx.h"
#include "material_manager.hpp"
#include "property_manager.hpp"
#include "utils.hpp"
#include "logger.hpp"

using namespace std;

MaterialManager* MaterialManager::_instance = nullptr;

bool MaterialManager::create() {
  assert(!_instance);
  _instance = new MaterialManager;
  return true;
}

bool MaterialManager::close() {
  assert(_instance);
  SAFE_DELETE(_instance);
  return true;
}

MaterialManager &MaterialManager::instance() {
  assert(_instance);
  return *_instance;
}


MaterialManager::MaterialManager() 
  : _materials(delete_obj<Material *>)
{
}

Material *MaterialManager::get_material(GraphicsObjectHandle obj) {
  return _materials.get(obj);
}

void MaterialManager::remove_material(const std::string &name) {
  assert(!"fix plz!");
  /*
  auto it = _materials.find(name);
  if (it == end(_materials))
  return;

  //_id_to_materials.erase((*it).second->id);
  //_materials.erase(it);
  */
}

GraphicsObjectHandle MaterialManager::find_material(const string &name) {
  int idx = _materials.idx_from_token(name);
  return idx == -1 ? GraphicsObjectHandle() : GraphicsObjectHandle(GraphicsObjectHandle::kMaterial, 0, idx);
}

GraphicsObjectHandle MaterialManager::add_material(Material *material, bool delete_existing) {

  const std::string &key = material->name();
  int idx = _materials.idx_from_token(key);
  if (idx != -1 || (idx = _materials.find_free_index()) != -1) {
    _materials.set_pair(idx, make_pair(key, material));
    return GraphicsObjectHandle(GraphicsObjectHandle::kMaterial, 0, idx);
  }
  return GraphicsObjectHandle();
}
