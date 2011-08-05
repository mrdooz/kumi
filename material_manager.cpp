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

uint16 MaterialManager::add_material(const Material &material) {
	uint16 key = _materials.size();
	_materials.push_back(material);
	return key;
}
