#pragma once
#include "graphics_submit.hpp"
#include "property.hpp"
#include "property_manager.hpp"

struct TrackedLocation;
class SubMesh;
class Mesh;

class SubMesh {
public:
  SubMesh(Mesh *mesh);
  ~SubMesh();
  void update();
  void fill_renderdata(MeshRenderData *render_data) const;
  RenderKey render_key() const { return _render_key; }
//private:

  std::vector<CBufferVariable> cbuffer_vars;
  std::vector<char> cbuffer_staged;

  std::string name;
  Mesh *mesh;
  RenderKey _render_key;
  GraphicsObjectHandle material_id;
  MeshGeometry geometry;
};

class Mesh {
public:
  Mesh(const std::string &name) : name(name) {}
  void submit(const TrackedLocation &location, int material_id, GraphicsObjectHandle technique);

  void update();
  void on_loaded();

  void fill_cbuffer(CBuffer *cbuffer) const;
//private:
  std::string name;
  XMFLOAT4X4 obj_to_world;
  std::vector<SubMesh *> submeshes;

  PropertyId _world_mtx_class;
  PropertyId _world_mtx_id;
  PropertyId _world_it_mtx_id;
};
