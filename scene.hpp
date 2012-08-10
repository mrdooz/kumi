#pragma once
#include "graphics_submit.hpp"

struct TrackedLocation;
class Mesh;
class Material;
class DeferredContext;

struct Camera {
  Camera(const std::string &name) : name(name) {}
  std::string name;
  PropertyId pos_id;
  PropertyId target_pos_id;
  XMFLOAT3 pos, target, up;
  float roll;
  float aspect_ratio;
  float fov_x, fov_y;
  float near_plane, far_plane;
  bool is_static;
};

struct Light {
  Light(const std::string &name) : name(name) {}
  std::string name;
  PropertyId pos_id;
  XMFLOAT4 pos;
  XMFLOAT4 color;
  float intensity;
  bool use_far_attenuation; 
  float far_attenuation_start;
  float far_attenuation_end;
  bool is_static;
};

struct Scene {

  Scene();
  ~Scene();

  void update();

  Mesh *find_mesh_by_name(const std::string &name);

  bool on_loaded();

  std::unordered_map<std::string, Mesh *> _meshes_by_name;

  void sort_by_material();

  void render(DeferredContext *ctx, GraphicsObjectHandle technique_handle);

  XMFLOAT4 ambient;
  std::vector<Mesh *> meshes;
  std::vector<Camera *> cameras;
  std::vector<Light *> lights;

  // TODO: move this to material mgr
  std::vector<Material *> materials;

  std::map<GraphicsObjectHandle, std::vector<SubMesh*>> _submesh_by_material;
};
