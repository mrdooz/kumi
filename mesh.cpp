#include "stdafx.h"
#include "mesh.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"
#include "logger.hpp"

SubMesh::SubMesh(Mesh *mesh) 
  : mesh(mesh)
{
  _render_key.cmd = RenderKey::kRenderMesh;
}

SubMesh::~SubMesh() {
}

void SubMesh::fill_renderdata(MeshRenderData *render_data) const {
  render_data->geometry = &geometry;
  render_data->material = material_id;
  render_data->mesh = mesh;
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

void Mesh::on_loaded() {
  _world_mtx_class = PROPERTY_MANAGER.get_or_create<int>("Mesh::world");
  _world_mtx_id = PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("world", this);
  _world_it_mtx_id = PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("world_it", this);
}
/*
void Mesh::submit(const TrackedLocation &location, int material_id, GraphicsObjectHandle technique) {

  for (size_t i = 0; i < submeshes.size(); ++i) {
    const SubMesh *submesh = submeshes[i];
    MeshRenderData *render_data = GRAPHICS.alloc_command_data<MeshRenderData>();
    render_data->technique = technique;
    submesh->fill_renderdata(render_data);
    GRAPHICS.submit_command(location, submesh->render_key(), render_data);
  }
}
*/
void Mesh::update() {
  for (auto i = begin(submeshes), e = end(submeshes); i != e; ++i)
    (*i)->update();
}

void Mesh::fill_cbuffer(CBuffer *cbuffer) const {
  for (size_t i = 0; i < cbuffer->vars.size(); ++i) {
    auto &cur = cbuffer->vars[i];
    if (cur.id == _world_mtx_class)
      PROPERTY_MANAGER.get_property_raw(_world_mtx_id, &cbuffer->staging[cur.ofs], cur.len);
  }
}
