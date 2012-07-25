#pragma once
#include "graphics_submit.hpp"
#include "property.hpp"
#include "property_manager.hpp"

struct TrackedLocation;
class SubMesh;
class Mesh;

class SubMesh {
  friend class KumiLoader;
public:
  SubMesh(Mesh *mesh);
  ~SubMesh();
  void update();
  void fill_renderdata(MeshRenderData *render_data) const;
  RenderKey render_key() const { return _render_key; }

  const std::string &name() const { return _name; }
  GraphicsObjectHandle material_id() const { return _material_id; }

  const MeshGeometry *geometry() const { return &_geometry; }
  Mesh *mesh() { return _mesh; }

private:

  std::vector<CBufferVariable> cbuffer_vars;
  std::vector<char> cbuffer_staged;

  std::string _name;
  Mesh *_mesh;
  RenderKey _render_key;
  GraphicsObjectHandle _material_id;
  MeshGeometry _geometry;
};

class Mesh {
  friend class KumiLoader;
public:
  Mesh(const std::string &name);
  void update();
  void on_loaded();

  void fill_cbuffer(CBuffer *cbuffer) const;
  PropertyId anim_id() const { return _anim_id; }
  const std::string &name() const { return _name; }

  const std::vector<SubMesh *> &submeshes() const { return _submeshes; }

private:
  std::string _name;
  XMFLOAT3 _pos;
  XMFLOAT4 _rot;
  XMFLOAT3 _scale;
  XMFLOAT4X4 _obj_to_world;
  std::vector<SubMesh *> _submeshes;

  XMFLOAT3 _center, _extents;

  bool _is_static;
  PropertyId _anim_id;
  PropertyId _world_mtx_class;
};
