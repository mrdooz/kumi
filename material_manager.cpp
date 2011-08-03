#include "stdafx.h"
#include "material_manager.hpp"

MaterialManager* MaterialManager::_instance = nullptr;

MaterialManager &MaterialManager::instance() {
	if (!_instance)
		_instance = new MaterialManager;
	return *_instance;
}


MaterialManager::MaterialManager() {

}

void MaterialManager::add_material(const Material &material) {
	_materials.insert(make_pair(material.name, material));
}
