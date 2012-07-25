#include "stdafx.h"
#include "mesh.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"
#include "logger.hpp"
#include "animation_manager.hpp"

SubMesh::SubMesh(Mesh *mesh) 
  : _mesh(mesh)
{
  _render_key.cmd = RenderKey::kRenderMesh;
}

SubMesh::~SubMesh() {
}

void SubMesh::fill_renderdata(MeshRenderData *render_data) const {
  render_data->geometry = &_geometry;
  render_data->material = _material_id;
  render_data->mesh = _mesh;
#if _DEBUG
  render_data->submesh = this;
#endif

}

void SubMesh::update() {
  if (!cbuffer_vars.empty()) {
    for (auto i = begin(cbuffer_vars), e = end(cbuffer_vars); i != e; ++i) {
      const CBufferVariable &var = *i;
      PROPERTY_MANAGER.get_property_raw(var.id, &cbuffer_staged[var.ofs], var.len);
    }
  }
}

Mesh::Mesh(const std::string &name) 
  : _name(name)
  , _is_static(true)
  , _anim_id(~0)
  , _world_mtx_class(~0)
{
}

void Mesh::on_loaded() {
  _world_mtx_class = PROPERTY_MANAGER.get_or_create<int>("Mesh::world");
  if (!_is_static) {
    _anim_id = PROPERTY_MANAGER.get_id(_name + "::Anim");
  }
}

void Mesh::update() {

  if (!_is_static) {
    uint32 flags;
    PosRotScale prs;
    ANIMATION_MANAGER.get_xform(_anim_id, &flags, &prs);
    _obj_to_world = compose_transform(
      flags & AnimationManager::kAnimPos ? prs.pos : _pos,
      flags & AnimationManager::kAnimRot ? prs.rot : _rot,
      flags & AnimationManager::kAnimScale ? prs.scale : _scale,
      true);
  }

  for (auto i = begin(_submeshes), e = end(_submeshes); i != e; ++i)
    (*i)->update();
}

void Mesh::fill_cbuffer(CBuffer *cbuffer) const {
  for (size_t i = 0; i < cbuffer->vars.size(); ++i) {
    auto &cur = cbuffer->vars[i];
    if (cur.id == _world_mtx_class) {
      memcpy(&cbuffer->staging[cur.ofs], (void *)&_obj_to_world, cur.len);
    }
  }
}
