#pragma once
#include "material.hpp"

class MaterialManager {
public:
	static MaterialManager &instance();

	uint16 find_material(const string &material_name);
	uint16 add_material(const Material &material);
	const Material &get_material(uint16 id);

private:
	MaterialManager();
	std::vector<Material> _materials;

	static MaterialManager *_instance;
};

#define MATERIAL_MANAGER MaterialManager::instance()