#pragma once

#include "graphics_object_handle.hpp"

struct Scene;
struct ResourceInterface;

class KumiLoader {
public:
  bool load(const char *filename, const char *material_connections, ResourceInterface *resource, Scene **scene);
private:
  bool load_meshes(const uint8 *buf, Scene *scene);
  bool load_cameras(const uint8 *buf, Scene *scene);
  bool load_lights(const uint8 *buf, Scene *scene);
  bool load_materials(const uint8 *buf, Scene *scene);
  bool load_animation(const uint8 *buf, Scene *scene);
  std::unordered_map<std::string, GraphicsObjectHandle> _material_ids;
  std::unordered_map<std::string, std::string> _technique_for_material;

  std::map<std::string, std::pair<std::string, std::string> > _material_overrides;
};