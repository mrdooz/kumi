#include "stdafx.h"
#include "mesh.hpp"
#include "renderer.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"

SubMesh::SubMesh(Mesh *mesh) : mesh(mesh)
{
  render_key.cmd = RenderKey::kRenderMesh;
#if _DEBUG
  render_data.mesh = mesh;
  render_data.submesh = this;
#endif
}

SubMesh::~SubMesh() {
}

void SubMesh::prepare_textures() {
  // get the pixel shader, check what textures he's using, and what slots to place them in
  // then we look these guys up in the current material
  Technique *technique = GRAPHICS.get_technique(render_data.technique);
  assert(technique);
  Shader *pixel_shader = technique->pixel_shader();
  auto &params = pixel_shader->resource_view_params();
  for (auto it = begin(params), e = end(params); it != e; ++it) {
    auto &name = it->name;
    auto &friendly_name = it->friendly_name;
    int bind_point = it->bind_point;
    render_data.textures[bind_point] = GRAPHICS.find_resource(friendly_name.c_str());
    render_data.first_texture = min(render_data.first_texture, bind_point);
    render_data.num_textures = max(render_data.num_textures, bind_point - render_data.first_texture + 1);
  }
}

void SubMesh::prepare_cbuffers() {

  Technique *technique = GRAPHICS.get_technique(render_data.technique);
  Shader *vs = technique->vertex_shader();
  Shader *ps = technique->pixel_shader();

  int cbuffer_size = 0;
  auto collect_params = [&, this](const std::vector<CBufferParam> &params) {
    for (auto i = std::begin(params), e = std::end(params); i != e; ++i) {
      const CBufferParam &param = *i;
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
            var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), mesh, var.len, nullptr);
            break;
          case PropertySource::kMaterial: {
            Material *material = MATERIAL_MANAGER.get_material(material_id);
            var.id = PROPERTY_MANAGER.get_or_create_raw(param.name.c_str(), material, var.len, nullptr);
            break;
          }
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

void Mesh::submit(const TrackedLocation &location, int material_id, GraphicsObjectHandle technique) {
  for (size_t i = 0; i < submeshes.size(); ++i) {
    SubMesh *s = submeshes[i];

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
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i) {
    (*i)->prepare_cbuffers();
    (*i)->prepare_textures();
  }
}

void Mesh::update() {
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i)
    (*i)->update();
}
