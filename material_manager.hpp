#pragma once
#include "property_manager.hpp"

struct MaterialProperty {

	MaterialProperty(const string &name, int value) : name(name), type(PropertyType::kInt), _int(value) {}
	MaterialProperty(const string &name, float value) : name(name), type(PropertyType::kFloat) { _float[0] = value; }
	MaterialProperty(const string &name, const XMFLOAT4 &value) : name(name), type(PropertyType::kFloat4) 
	{ _float[0] = value.x; _float[1] = value.z; _float[2] = value.y; _float[3] = value.w; }

	string name;
	PropertyType::Enum type;
	union {
		int _int;
		float _float[4];
	};
};

struct Material {
	Material(const string &technique, const string &name) : technique(technique), name(name) {}
	string technique;
	string name;
	std::vector<MaterialProperty> properties;
};


class MaterialManager {
public:
	static MaterialManager &instance();

	uint16 add_material(const Material &material);
	const Material &get_material(uint16 id);
private:
	MaterialManager();
	std::vector<Material> _materials;

	static MaterialManager *_instance;
};

#define MATERIAL_MANAGER MaterialManager::instance()