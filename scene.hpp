#pragma once
#include "graphics_submit.hpp"

struct TrackedLocation;
class Mesh;
class Material;

struct Camera {
  Camera(const std::string &name) : name(name) {}
  std::string name;
  XMFLOAT3 pos, target, up;
  float roll;
  float aspect_ratio;
  float fov_x, fov_y;
  float near_plane, far_plane;
};

struct Light {
  Light(const std::string &name) : name(name) {}
  std::string name;
  XMFLOAT4 pos;
  XMFLOAT4 color;
  float intensity;
  bool use_far_attenuation; 
  float far_attenuation_start;
  float far_attenuation_end;
};

#pragma pack(push, 1)
template <typename T>
struct KeyFrame {
  double time;
  T value;
};

typedef KeyFrame<float> KeyFrameFloat;
typedef KeyFrame<XMFLOAT3> KeyFrameVec3;
typedef KeyFrame<XMFLOAT4X4> KeyFrameMatrix;

#pragma pack(pop)

struct Scene {

  ~Scene();

  void update();

  void submit_meshes(const TrackedLocation &location, int material_id = -1, GraphicsObjectHandle technique = GraphicsObjectHandle());
  void submit_mesh(const TrackedLocation &location, const char *name, int material_id = -1, GraphicsObjectHandle technique = GraphicsObjectHandle());

  Mesh *find_mesh_by_name(const std::string &name);

  bool on_loaded();

  std::unordered_map<std::string, Mesh *> _meshes_by_name;

  void sort_by_material();

  XMFLOAT4 ambient;
  std::vector<Mesh *> meshes;
  std::vector<Camera *> cameras;
  std::vector<Light *> lights;
  std::vector<Material *> materials;
  std::map<std::string, std::vector<KeyFrameFloat>> animation_float;
  std::map<std::string, std::vector<KeyFrameVec3>> animation_vec3;
  std::map<std::string, std::vector<KeyFrameMatrix>> animation_mtx;

  std::map<GraphicsObjectHandle, std::vector<SubMesh*>> _submesh_by_material;
};
