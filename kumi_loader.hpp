#pragma once

struct Scene;

class KumiLoader {
public:
	bool load(const char *filename, Scene **scene);
private:
	bool load_meshes(const char *buf, Scene *scene);
	bool load_cameras(const char *buf, Scene *scene);
	bool load_lights(const char *buf, Scene *scene);
	bool load_materials(const char *buf);
	std::hash_map<string, uint16> _material_ids;
};