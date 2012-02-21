#include "stdafx.h"
#include "material_manager.hpp"
#include "property_manager.hpp"
#include "utils.hpp"

using namespace std;

MaterialManager* MaterialManager::_instance = nullptr;

MaterialManager &MaterialManager::instance() {
  if (!_instance)
    _instance = new MaterialManager;
  return *_instance;
}


MaterialManager::MaterialManager() : _next_material_id(0) {

}

const Material *MaterialManager::get_material(uint16 id) {
  return _id_to_materials[id];
}

void MaterialManager::remove_material(const std::string &name) {
  auto it = find(begin(_materials), end(_materials), [&](const Material *m) { return m->name == name; });
  if (it == end(_materials))
    return;

  _id_to_materials.erase((*it).second->id);
  _materials.erase(it);
}

uint16 MaterialManager::find_material(const string &name) {
  auto it = find(begin(_materials), end(_materials), [&](const Material *m) { return m->name == name; });
  return it != end(_materials) ? it->second->id : -1;
}

uint16 MaterialManager::add_material(const Material *material, bool delete_existing) {

  // don't allow duplicate material names. this shouldn't impose any real limitations, as material names are
  // prefixed with the technique name when they are loaded
  auto it = find(begin(_materials), end(_materials), [&](const Material *m) { return m->name == material->name; });
  uint16 prev_key = ~0;
  if (it != end(_materials)) {
    if (delete_existing) {
      prev_key = (*it).second->id;
      _id_to_materials.erase(prev_key);
      _materials.erase(it);
    } else {
      LOG_ERROR_LN("Duplicate material added: %s", material->name.c_str());
    }
  }

  uint16 key = prev_key == ~0 ? _next_material_id++ : prev_key;
  material->id = key;
  _materials.insert(make_pair(material->name, unique_ptr(material)));
  _materials.push_back(material);

  for (size_t i = 0; i < material.properties.size(); ++i) {
    const MaterialProperty &p = material.properties[i];
    switch (p.type) {
    case PropertyType::kFloat:
      PROPERTY_MANAGER.set_material_property(key, p.name.c_str(), p._float[0]);
      break;
    case PropertyType::kFloat4:
      PROPERTY_MANAGER.set_material_property(key, p.name.c_str(), XMFLOAT4(p._float[0], p._float[1], p._float[2], p._float[3]));
      break;
    }
  }

  return key;
}
