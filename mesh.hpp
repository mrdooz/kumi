#pragma once
#include "graphics_submit.hpp"
#include "property.hpp"
#include "property_manager.hpp"

struct TrackedLocation;
struct SubMesh;
struct Mesh;

struct SubMesh {
  SubMesh(Mesh *mesh);
  ~SubMesh();

  void update();
  void prepare_cbuffers(GraphicsObjectHandle technique_handle);
  void prepare_textures(GraphicsObjectHandle technique_handle);

  // low word: 
  // 0 not found, free idx in HIWORD
  // 1 found technique at idx HIWORD
  // 2 too many techniques
  uint32 find_technique_index(GraphicsObjectHandle technique);

  struct CBufferVariable {
    int ofs;
    int len;
    PropertyId id;
  };

  std::vector<CBufferVariable> cbuffer_vars;
  std::vector<uint8> cbuffer_staged;

  std::string name;
  Mesh *mesh;
  RenderKey render_key;
  MeshRenderData render_data;
  GraphicsObjectHandle material_id;
};

struct Mesh {

  Mesh(const std::string &name) : name(name) {}
  void submit(const TrackedLocation &location, int material_id, GraphicsObjectHandle technique);

  void prepare_cbuffer();
  void update();

  void fill_cbuffer(CBuffer *cbuffer);

  std::string name;
  XMFLOAT4X4 obj_to_world;
  std::vector<SubMesh *> submeshes;

  PropertyId _world_mtx_class;
  PropertyId _world_mtx_id;
  PropertyId _world_it_mtx_id;
};
