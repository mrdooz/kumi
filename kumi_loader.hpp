#pragma once

#include "graphics_object_handle.hpp"
#include "path_utils.hpp"

struct Scene;
struct ResourceInterface;

class KumiLoader {
public:
  bool load(const char *filename, const char *material_override, ResourceInterface *resource, Scene **scene);
private:
  bool load_meshes(const char *buf, Scene *scene);
  bool load_cameras(const char *buf, Scene *scene);
  bool load_lights(const char *buf, Scene *scene);
  bool load_materials(const char *buf, Scene *scene);
  bool load_animation(const char *buf, Scene *scene);
  std::unordered_map<std::string, GraphicsObjectHandle> _material_name_to_id;
  std::unordered_map<std::string, std::string> _technique_for_material;

  std::map<std::string, std::pair<std::string, std::string> > _material_overrides;
  string _filename;
};