#pragma once
#include "material.hpp"

class MaterialManager {
public:
	static MaterialManager &instance();

	uint16 find_material(const string &name);
	uint16 add_material(const Material *material, bool delete_existing);
  void remove_material(const std::string &name);
	const Material *get_material(uint16 id);

private:
	MaterialManager();
	std::map<string, std::unique_ptr<Material> > _materials;
  std::map<uint16, Material *> _id_to_materials;  // weak ptr to _materials
	static MaterialManager *_instance;
  uint16 _next_material_id;
};

#define MATERIAL_MANAGER MaterialManager::instance()