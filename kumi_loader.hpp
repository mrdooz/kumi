#pragma once

struct Scene;
struct ResourceInterface;

class KumiLoader {
public:
	bool load(const char *filename, ResourceInterface *resource, Scene **scene);
private:
	bool load_meshes(const char *buf, Scene *scene);
	bool load_cameras(const char *buf, Scene *scene);
	bool load_lights(const char *buf, Scene *scene);
	bool load_materials(const char *buf);
	std::unordered_map<string, uint16> _material_ids;
	std::unordered_map<string, string> _default_techniques;
};