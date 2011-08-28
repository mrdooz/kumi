#pragma once

using std::hash_map;

namespace Property {
	enum Enum {
		kUnknown,
		kFloat,
		kFloat2,
		kFloat3,
		kFloat4,
		kFloat4x4,
		kTexture2d,
		kSampler,
		kInt,
	};
}

class PropertyManager {
public:
	static PropertyManager& instance();

	typedef intptr_t Id;

	template<typename T> void set_system_property(const char *name, const T &value) {
		BagGetter<T> b(_system_properties);
		b(name) = value;
	}

	template<typename T> T get_system_property(const char *name) {
		BagGetter<T> b(_system_properties);
		return b(name);
	}

	template<typename T> void set_material_property(Id material_id, const char *name, const T &value) {
		BagGetter<T> b(_material_properties[material_id]);
		b(name) = value;
	}

	template<typename T> T get_material_property(Id material_id, const char *name) {
		BagGetter<T> b(_material_properties[material_id]);
		return b(name);
	}

	template<typename T> void set_mesh_property(Id mesh_id, const char *name, const T &value) {
		BagGetter<T> b(_mesh_properties[mesh_id]);
		b(name) = value;
	}

	template<typename T> T get_mesh_property(Id mesh_id, const char *name) {
		BagGetter<T> b(_mesh_properties[mesh_id]);
		return b(name);
	}

private:

	struct PropertyBag {
		hash_map<string, float> float_values;
		hash_map<string, XMFLOAT4> float4_values;
		hash_map<string, XMFLOAT4X4> matrix_values;
	};

	template<typename T>
	struct BagGetter;

	template<> struct BagGetter<float> {
		BagGetter(PropertyBag &bag) : bag(bag) {}
		float &operator()(const char *name) { return bag.float_values[name]; }
		PropertyBag &bag;
	};

	template<> struct BagGetter<XMFLOAT4> {
		BagGetter(PropertyBag &bag) : bag(bag) {}
		XMFLOAT4 &operator()(const char *name) { return bag.float4_values[name]; }
		PropertyBag &bag;
	};

	template<> struct BagGetter<XMFLOAT4X4> {
		BagGetter(PropertyBag &bag) : bag(bag) {}
		XMFLOAT4X4 &operator()(const char *name) { return bag.matrix_values[name]; }
		PropertyBag &bag;
	};

	PropertyBag _system_properties;
	hash_map<Id, PropertyBag> _material_properties;
	hash_map<Id, PropertyBag> _mesh_properties;

	PropertyManager();
	static PropertyManager *_instance;
};

#define PROPERTY_MANAGER PropertyManager::instance()
