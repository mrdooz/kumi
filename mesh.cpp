#include "stdafx.h"
#include "mesh.hpp"
#include "renderer.hpp"
#include "graphics.hpp"
#include "technique.hpp"

SubMesh::SubMesh(Mesh *mesh)
{
  render_key.cmd = RenderKey::kRenderMesh;
  render_data.mesh = mesh;
  render_data.submesh = this;
}

SubMesh::~SubMesh() {
}

void SubMesh::prepare_cbuffers() {
  Technique *technique = GRAPHICS.get_technique(render_data.technique);
  Shader *vs = technique->vertex_shader();
  Shader *ps = technique->pixel_shader();

  int cbuffer_size = 0;
  auto collect_params = [&, this](const std::vector<CBufferParam> &params) {
    for (auto j = std::begin(params), e = std::end(params); j != e; ++j) {
      const CBufferParam &param = *j;
      int len = PropertyType::len(param.type);;
      if (param.cbuffer != -1) {
        SubMesh::CBufferVariable &var = dummy_push_back(&cbuffer_vars);
        var.ofs = param.start_offset;
        var.len = len;
        switch (param.source) {
        case PropertySource::kSystem:
          var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), var.len, nullptr);
          break;
        case PropertySource::kMesh:
          var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), this, var.len, nullptr);
          break;
        case PropertySource::kMaterial:
          var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), (PropertyManager::Token)material_id, var.len, nullptr);
          break;
        }
      }
      cbuffer_size += len;
    }
  };

  collect_params(vs->cbuffer_params());
  collect_params(ps->cbuffer_params());
  cbuffer_staged.resize(cbuffer_size);

}

void SubMesh::update() {
  for (auto i = begin(cbuffer_vars), e = end(cbuffer_vars); i != e; ++i) {
    const CBufferVariable &var = *i;
    PROPERTY_MANAGER.get_property_raw(var.id, &cbuffer_staged[var.ofs], var.len);
  }
  render_data.cbuffer_staged = &cbuffer_staged[0];
  render_data.cbuffer_len = cbuffer_staged.size();
}

void Mesh::submit(const TrackedLocation &location, uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
  for (size_t i = 0; i < submeshes.size(); ++i) {
    SubMesh *s = submeshes[i];
    s->render_key.seq_nr = seq_nr;

    // make a copy of the render data if we're using a material or technique override
    bool material_override = material_id != -1;
    bool technique_override = technique.is_valid();
    MeshRenderData *p;
    if (material_override || technique_override) {
      p = RENDERER.alloc_command_data<MeshRenderData>();
      *p = s->render_data;
      // TODO: handle override by creating a tmp cbuffer
      //if (material_override)
        //p->material_id = (uint16)material_id;
      if (technique_override)
        p->technique = technique;
    } else {
      p = &s->render_data;
    }
    RENDERER.submit_command(location, s->render_key, p);
  }
}


void Mesh::prepare_cbuffer() {

  _world_mtx_id = PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("world", this);

  // collect all the variables we need to fill our cbuffer.
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i)
    (*i)->prepare_cbuffers();
}

void Mesh::update() {
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i)
    (*i)->update();
}
