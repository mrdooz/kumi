#include "stdafx.h"
#include "mesh.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"
#include "logger.hpp"
#include "animation_manager.hpp"
#include "deferred_context.hpp"

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

Mesh::~Mesh() {
  seq_delete(&_submeshes);
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

void Mesh::render(DeferredContext *ctx, GraphicsObjectHandle technique_handle) {

  Technique *technique = GRAPHICS.get_technique(technique_handle);

  ctx->set_rs(technique->rasterizer_state());
  ctx->set_dss(technique->depth_stencil_state(), GRAPHICS.default_stencil_ref());
  ctx->set_bs(technique->blend_state(), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

  for (size_t i = 0; i < _submeshes.size(); ++i) {
    SubMesh *submesh = _submeshes[i];

    const MeshGeometry *geometry = submesh->geometry();
    const Material *material = MATERIAL_MANAGER.get_material(submesh->material_id());

    // get the shader for the current technique, based on the flags used by the material
    int flags = material->flags();
    Shader *vs = technique->vertex_shader(flags);
    Shader *ps = technique->pixel_shader(flags);

    ctx->set_vs(vs->handle());
    ctx->set_ps(ps->handle());

    ctx->set_layout(vs->input_layout());
    ctx->set_vb(geometry->vb, geometry->vertex_size);
    ctx->set_ib(geometry->ib, geometry->index_format);
    ctx->set_topology(geometry->topology);

    // set cbuffers
    fill_cbuffer(&vs->mesh_cbuffer());
    material->fill_cbuffer(&vs->material_cbuffer());
    technique->fill_cbuffer(&vs->system_cbuffer());

    fill_cbuffer(&ps->mesh_cbuffer());
    material->fill_cbuffer(&ps->material_cbuffer());
    technique->fill_cbuffer(&ps->system_cbuffer());

    ctx->set_cbuffers(vs->cbuffers(), ps->cbuffers());

    // set samplers
    auto &samplers = ps->samplers();
    if (!samplers.empty()) {
      ctx->set_samplers(samplers);
    }

    // set resource views
    auto &rv = ps->resource_views();
    TextureArray all_views;
    ctx->fill_system_resource_views(rv, &all_views);
    material->fill_resource_views(rv, &all_views);
    bool has_resources = false;
    for (size_t i = 0; i < all_views.size(); ++i) {
      if (all_views[i].is_valid()) {
        has_resources = true;
        ctx->set_shader_resources(all_views);
        break;
      }
    }

    ctx->draw_indexed(geometry->index_count, 0, 0);

    if (has_resources)
      ctx->unset_shader_resource(0, MAX_TEXTURES);
  }
}
