#pragma once

struct Scene;
struct ResourceInterface;

class KumiLoader {
public:
	bool load(const char *filename, ResourceInterface *resource, Scene **scene);
private:
	bool load_meshes(const uint8 *buf, Scene *scene);
	bool load_cameras(const uint8 *buf, Scene *scene);
	bool load_lights(const uint8 *buf, Scene *scene);
	bool load_materials(const uint8 *buf);
	std::unordered_map<std::string, uint16> _material_ids;
	std::unordered_map<std::string, std::string> _default_techniques;
};