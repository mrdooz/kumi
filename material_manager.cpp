#include "stdafx.h"
#include "material_manager.hpp"
#include "property_manager.hpp"
#include "utils.hpp"
#include "logger.hpp"

using namespace std;

MaterialManager* MaterialManager::_instance = nullptr;

bool MaterialManager::create() {
  KASSERT(!_instance);
  _instance = new MaterialManager;
  return true;
}

bool MaterialManager::close() {
  delete exch_null(_instance);
  return true;
}

MaterialManager &MaterialManager::instance() {
  KASSERT(_instance);
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
  KASSERT(!"fix plz!");
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

GraphicsObjectHandle MaterialManager::add_material(Material *material, bool replace_existing) {

  const std::string &name = material->name();
  int old_idx = _materials.idx_from_token(name);
  int new_idx = _materials.find_free_index();

  if (old_idx != -1 && replace_existing || old_idx == -1 && new_idx != -1) {
    int idx = old_idx != -1 ? old_idx : new_idx;      
    _materials.set_pair(idx, make_pair(name, material));
    return GraphicsObjectHandle(GraphicsObjectHandle::kMaterial, 0, idx);
  }
  return GraphicsObjectHandle();
}
