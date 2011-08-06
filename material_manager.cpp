#include "stdafx.h"
#include "material_manager.hpp"
#include "property_manager.hpp"

MaterialManager* MaterialManager::_instance = nullptr;

MaterialManager &MaterialManager::instance() {
	if (!_instance)
		_instance = new MaterialManager;
	return *_instance;
}


MaterialManager::MaterialManager() {

}

const Material &MaterialManager::get_material(uint16 id) {
	assert(id < _materials.size());
	return _materials[id];
}

uint16 MaterialManager::add_material(const Material &material) {
	uint16 key = _materials.size();
	_materials.push_back(material);

	for (size_t i = 0; i < material.properties.size(); ++i) {
		const MaterialProperty &p = material.properties[i];
		switch (p.type) {
		case Property::kFloat:
			PROPERTY_MANAGER.set_material_property(key, p.name.c_str(), p._float[0]);
			break;
		case Property::kFloat4:
			PROPERTY_MANAGER.set_material_property(key, p.name.c_str(), XMFLOAT4(p._float[0], p._float[1], p._float[2], p._float[3]));
			break;
		}
	}

	return key;
}
