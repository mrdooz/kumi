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
  : _next_material_id(0) {
}

const Material *MaterialManager::get_material(uint16 id) {
  return _id_to_materials[id];
}

void MaterialManager::remove_material(const std::string &name) {
  auto it = _materials.find(name);
  if (it == end(_materials))
    return;

  assert(!"fix plz!");
  //_id_to_materials.erase((*it).second->id);
  //_materials.erase(it);
}

uint16 MaterialManager::find_material(const string &name) {
  auto it = _materials.find(name);
  return it != end(_materials) ? (uint16)it->second->id : -1;
}

uint16 MaterialManager::add_material(Material *material, bool delete_existing) {

  // don't allow duplicate material names. this shouldn't impose any real limitations, as material names are
  // prefixed with the technique name when they are loaded
  auto it = _materials.find(material->name);
  uint16 prev_key = ~0;
  if (it != end(_materials)) {
    if (delete_existing) {
      prev_key = (uint16)(*it).second->id;
      _id_to_materials.erase(prev_key);
      _materials.erase(it);
    } else {
      LOG_ERROR_LN("Duplicate material added: %s", material->name.c_str());
    }
  }

  uint16 key = prev_key == (uint16)~0 ? _next_material_id++ : prev_key;
  material->id = (PropertyManager::Token)key;
  _materials.insert(make_pair(material->name, unique_ptr<Material>(material)));

  for (auto i = begin(material->properties); i != end(material->properties); ++i) {
    const Material::Property &p = *i;
    switch (p.type) {
    case PropertyType::kFloat:
      PROPERTY_MANAGER.set_property<float>(p.id, p.value.x);
      break;
    case PropertyType::kFloat4:
      PROPERTY_MANAGER.set_property<XMFLOAT4>(p.id, p.value);
      break;
    }
  }

  _id_to_materials[key] = material;

  return key;
}
