#pragma once
#include "material.hpp"
#include "id_buffer.hpp"
#include "graphics_object_handle.hpp"

class MaterialManager {
public:
  static MaterialManager &instance();
  static bool create();
  static bool close();

  GraphicsObjectHandle find_material(const std::string &name);
  GraphicsObjectHandle add_material(Material *material, bool delete_existing);
  void remove_material(const std::string &name);
  Material *get_material(GraphicsObjectHandle id);

private:
  MaterialManager();
  static MaterialManager *_instance;

  SearchableIdBuffer<std::string, Material *, 1024> _materials;

};

#define MATERIAL_MANAGER MaterialManager::instance()