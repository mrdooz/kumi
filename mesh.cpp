#include "stdafx.h"
#include "mesh.hpp"
#include "renderer.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"
#include "logger.hpp"

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

uint32 SubMesh::find_technique_index(GraphicsObjectHandle technique) {
  for (int i = 0; i < 16; ++i) {
    if (!render_data.technique_data[i].technique.is_valid())
      return MAKEWORD(0, i);
    if (render_data.technique_data[i].technique == technique) {
      return MAKEWORD(1, i);
    }
  }
  LOG_ERROR_LN("Max # techniques used for submesh! About to croak!");
  return MAKEWORD(2, 0);
}

void SubMesh::prepare_textures(GraphicsObjectHandle technique_handle) {
  // get the pixel shader, check what textures he's using, and what slots to place them in
  // then we look these guys up in the current material
  Technique *technique = GRAPHICS.get_technique(technique_handle);
  if (!technique)
    return;
  uint32 res = find_technique_index(technique_handle);
  // bail if at max # techniques, or we've already prepared that techinque
  if (LOWORD(res) > 0)
    return;
  int idx = HIWORD(res);

  auto data = &render_data.technique_data[idx];
  Shader *pixel_shader = technique->pixel_shader();
  auto &params = pixel_shader->resource_view_params();
  for (auto it = begin(params), e = end(params); it != e; ++it) {
    auto &name = it->name;
    auto &friendly_name = it->friendly_name;
    int bind_point = it->bind_point;
    data->textures[bind_point] = GRAPHICS.find_resource(friendly_name.c_str());
    data->first_texture = min(data->first_texture, bind_point);
    data->num_textures = max(data->num_textures, bind_point - data->first_texture + 1);
  }
}

void SubMesh::prepare_cbuffers(GraphicsObjectHandle technique_handle) {

  Technique *technique = GRAPHICS.get_technique(technique_handle);
  if (!technique)
    return;

  uint32 res = find_technique_index(technique_handle);
  // bail if at max # techniques, or we've already prepared that techinque
  if (LOWORD(res) > 0)
    return;
  int idx = HIWORD(idx);

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
  if (!cbuffer_vars.empty()) {
    for (auto i = begin(cbuffer_vars), e = end(cbuffer_vars); i != e; ++i) {
      const CBufferVariable &var = *i;
      PROPERTY_MANAGER.get_property_raw(var.id, &cbuffer_staged[var.ofs], var.len);
    }
    render_data.cur_technique_data->cbuffer_staged = &cbuffer_staged[0];
    render_data.cur_technique_data->cbuffer_len = cbuffer_staged.size();
  }
}

void Mesh::submit(const TrackedLocation &location, int material_id, GraphicsObjectHandle technique) {
  for (size_t i = 0; i < submeshes.size(); ++i) {
    SubMesh *s = submeshes[i];

    // make a copy of the render data if we're using a material or technique override
    bool material_override = material_id != -1;
    bool technique_override = technique.is_valid();
    MeshRenderData *p;
    int idx;
    if (material_override || technique_override) {
      // I'm not really sure how to handle material overrides, so i'll just defer it :)
      // p = RENDERER.alloc_command_data<MeshRenderData>();
      // TODO: handle override by creating a tmp cbuffer
      //if (material_override)
        //p->material_id = (uint16)material_id;
      if (technique_override) {
        s->prepare_cbuffers(technique);
        s->prepare_textures(technique);
        s->render_data.cur_technique = technique;
        idx = s->find_technique_index(technique);
      }
      //*p = s->render_data;
    } else {
      idx = s->find_technique_index(s->render_data.cur_technique);
    }
    s->render_data.cur_technique_data = &s->render_data.technique_data[idx];
    p = &s->render_data;
    RENDERER.submit_command(location, s->render_key, p);
  }
}


void Mesh::prepare_cbuffer() {

  _world_mtx_id = PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("world", this);

  // collect all the variables we need to fill our cbuffer.
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i) {
    SubMesh *submesh = *i;
    auto rd = &submesh->render_data;
    auto technique = rd->cur_technique;
    submesh->prepare_cbuffers(technique);
    submesh->prepare_textures(technique);
    rd->cur_technique_data = &rd->technique_data[HIWORD(submesh->find_technique_index(technique))];
  }
}


void Mesh::update() {
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i)
    (*i)->update();
}
