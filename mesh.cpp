#include "stdafx.h"
#include "mesh.hpp"
#include "renderer.hpp"
#include "graphics.hpp"
#include "technique.hpp"

SubMesh::SubMesh()
{
  render_key.cmd = RenderKey::kRenderMesh;
}

SubMesh::~SubMesh() {
  render_key.cmd = RenderKey::kDelete;
  RENDERER.submit_command(FROM_HERE, render_key, (void *)&render_data);
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
      if (material_override)
        p->material_id = (uint16)material_id;
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
  for (size_t i = 0; i < submeshes.size(); ++i) {
    SubMesh *submesh = submeshes[i];
    Technique *technique = GRAPHICS.get_technique(submeshes[i]->render_data.technique);
    Shader *vs = technique->vertex_shader();
    Shader *ps = technique->pixel_shader();

    int cbuffer_size = 0;
    auto collect_params = [&](const std::vector<CBufferParam> &params) {
      for (auto j = begin(params), e = end(params); j != e; ++j) {
        const CBufferParam &param = *j;
        int len = PropertyType::len(param.type);;
        if (param.cbuffer != -1) {
          SubMesh::CBufferVariable &var = dummy_push_back(&submesh->cbuffer_vars);
          var.ofs = param.start_offset;
          var.len = len;
          switch (param.source) {
          case PropertySource::kSystem:
            var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), var.len, nullptr);
            break;
          case PropertySource::kMesh:
            var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), (PropertyManager::Token)submesh->render_data.mesh_id, var.len, nullptr);
            break;
          case PropertySource::kMaterial:
            var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), (PropertyManager::Token)submesh->render_data.material_id, var.len, nullptr);
            break;
          }
        }
        cbuffer_size += len;
      }
    };

    collect_params(vs->cbuffer_params());
    collect_params(ps->cbuffer_params());
    submesh->cbuffer_staged.resize(cbuffer_size);
  }
}

void Mesh::update() {
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i)
    (*i)->update();
}
