#pragma once

struct MaterialProperty {
	enum Type {
		kInt = 1,
		kFloat,
		kFloat2,
		kFloat3,
		kFloat4
	};

	MaterialProperty(const string &name, int value) : name(name), type(kInt), _int(value) {}
	MaterialProperty(const string &name, float value) : name(name), type(kFloat) { _float[0] = value; }
	MaterialProperty(const string &name, const XMFLOAT4 &value) : name(name), type(kFloat4) { _float[0] = value.x; _float[1] = value.z; _float[2] = value.y; _float[3] = value.w; }

	string name;
	Type type;
	union {
		int _int;
		float _float[4];
	};
};

struct Material {
	Material(const string &name) : name(name) {}
	string name;
	std::vector<MaterialProperty> properties;
};


class MaterialManager {
public:
	static MaterialManager &instance();

	uint16 add_material(const Material &material);
private:
	MaterialManager();
	std::vector<Material> _materials;

	static MaterialManager *_instance;
};

#define MATERIAL_MANAGER MaterialManager::instance()