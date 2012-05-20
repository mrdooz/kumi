#pragma once
#include "graphics_submit.hpp"

struct TrackedLocation;
struct Mesh;
struct Material;

struct Camera {
  Camera(const std::string &name) : name(name) {}
  std::string name;
  XMFLOAT3 pos, target, up;
  float roll;
  float aspect_ratio;
  float fov;
  float near_plane, far_plane;
};

struct Light {
  Light(const std::string &name) : name(name) {}
  std::string name;
  XMFLOAT4 pos;
  XMFLOAT4 color;
  float intensity;
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

  void submit_meshes(const TrackedLocation &location, uint16 seq_nr, int material_id = -1, GraphicsObjectHandle technique = GraphicsObjectHandle());
  void submit_mesh(const TrackedLocation &location, const char *name, uint16 seq_nr, int material_id = -1, GraphicsObjectHandle technique = GraphicsObjectHandle());

  Mesh *find_mesh_by_name(const std::string &name);

  bool on_loaded();

  std::unordered_map<std::string, Mesh *> _meshes_by_name;

  std::vector<Mesh *> meshes;
  std::vector<Camera *> cameras;
  std::vector<Light *> lights;
  std::vector<Material *> materials;
  std::map<std::string, std::vector<KeyFrameFloat>> animation_float;
  std::map<std::string, std::vector<KeyFrameVec3>> animation_vec3;
  std::map<std::string, std::vector<KeyFrameMatrix>> animation_mtx;
};
