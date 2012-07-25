#pragma once

#include "graphics_object_handle.hpp"
#include "path_utils.hpp"

class Mesh;
struct Scene;
struct ResourceInterface;
class BitReader;

class KumiLoader {
public:
  bool load(const char *filename, const char *material_override, ResourceInterface *resource, Scene **scene);
private:

#pragma pack(push, 1)
  struct MainHeader {
    int version;
    int position_bits;
    int normal_bits;
    int texcoord_bits;
    int animation_bits;
    int global_ofs;
    int material_ofs;
    int mesh_ofs;
    int light_ofs;
    int camera_ofs;
    int animation_ofs;
    int binary_ofs;
    int total_size;
  };
#pragma pack(pop)

  bool load_globals(const char *buf, Scene *scene);
  bool load_meshes(const char *buf, Scene *scene);
  bool load_cameras(const char *buf, Scene *scene);
  bool load_lights(const char *buf, Scene *scene);
  bool load_materials(const char *buf, Scene *scene);
  bool load_animation(const char *buf, Scene *scene);
  bool load_animation2(const char *buf, Scene *scene);
  bool load_animation3(const char *buf, Scene *scene);
  std::unordered_map<std::string, GraphicsObjectHandle> _material_name_to_id;
  std::unordered_map<std::string, std::string> _technique_for_material;

  void decompress_ib(BitReader *reader, int num_indices, int index_size, std::vector<char> *out);
  void decompress_vb(Mesh *mesh, BitReader *reader, int num_verts, int vertex_size, int vb_flags, std::vector<char> *out);

  MainHeader _header;
  std::map<std::string, std::pair<std::string, std::string> > _material_overrides;
  string _filename;
};