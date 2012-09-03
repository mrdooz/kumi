#include "stdafx.h"
#include "scene.hpp"
#include "graphics.hpp"
#include "mesh.hpp"
#include "material.hpp"
#include "animation_manager.hpp"
#include "deferred_context.hpp"
#include "material_manager.hpp"
#include "effect.hpp"

Scene::Scene() {
}

Scene::~Scene() {
  seq_delete(&meshes);
  seq_delete(&cameras);
  seq_delete(&lights);
  //seq_delete(&materials);
}

Mesh *Scene::find_mesh_by_name(const std::string &name) {
  auto it = _meshes_by_name.find(name);
  return it != _meshes_by_name.end() ? it->second : nullptr;
}

bool Scene::on_loaded() {
  for (auto it = begin(meshes), e = end(meshes); it != e; ++it) {
    Mesh *mesh = *it;
    mesh->on_loaded();
    _meshes_by_name[mesh->name()] = mesh;
  }

  for (size_t i = 0; i < cameras.size(); ++i) {
    auto *camera = cameras[i];
    if (!camera->is_static) {
      camera->pos_id = PROPERTY_MANAGER.get_id(camera->name + "::Anim");
      camera->target_pos_id = PROPERTY_MANAGER.get_id(camera->name + ".Target::Anim");
    }
  }

  for (size_t i = 0; i < lights.size(); ++i) {
    auto *light = lights[i];
    if (!light->is_static)
      light->pos_id = PROPERTY_MANAGER.get_id(light->name + "::Anim");
  }

  sort_by_material();

  return true;
}

void Scene::update() {
  for (auto i = begin(meshes), e = end(meshes); i != e; ++i)
    (*i)->update();
}

void Scene::sort_by_material() {

  for (size_t i = 0; i < meshes.size(); ++i) {
    auto &submeshes = meshes[i]->submeshes();
    for (size_t j = 0; j < submeshes.size(); ++j) {
      auto &submesh = submeshes[j];
      _submesh_by_material[submesh->material_id()].push_back(submesh);
    }
  }
}

void Scene::render(DeferredContext *ctx, Effect *effect, GraphicsObjectHandle technique_handle) {

  Technique *technique = GRAPHICS.get_technique(technique_handle);
  ctx->set_rs(technique->rasterizer_state());
  ctx->set_dss(technique->depth_stencil_state(), GRAPHICS.default_stencil_ref());
  ctx->set_bs(technique->blend_state(), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

  for (auto it = begin(_submesh_by_material); it != end(_submesh_by_material); ++it) {
    GraphicsObjectHandle m = it->first;
    const Material *material = MATERIAL_MANAGER.get_material(m);

    // get the shader for the current technique, based on the flags used by the material
    int flags = material->flags();
    Shader *vs = technique->vertex_shader(flags);
    Shader *ps = technique->pixel_shader(flags);
    ctx->set_layout(vs->input_layout());

    material->fill_cbuffer(&vs->material_cbuffer());
    material->fill_cbuffer(&ps->material_cbuffer());
    ctx->set_cbuffer(vs->material_cbuffer(), ps->material_cbuffer());


    effect->fill_cbuffer(&vs->system_cbuffer());
    effect->fill_cbuffer(&ps->system_cbuffer());
    ctx->set_cbuffer(vs->system_cbuffer(), ps->system_cbuffer());

    ctx->set_vs(vs->handle());
    ctx->set_ps(ps->handle());

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
        ctx->set_shader_resources(all_views, ShaderType::kPixelShader);
        break;
      }
    }

    for (auto jt = begin(it->second); jt != end(it->second); ++jt) {
      SubMesh *submesh = *jt;
      const MeshGeometry *geometry = submesh->geometry();
      Mesh *mesh = submesh->mesh();

      ctx->set_vb(geometry->vb);
      ctx->set_ib(geometry->ib);
      ctx->set_topology(geometry->topology);

      // set cbuffers
      mesh->fill_cbuffer(&vs->mesh_cbuffer());
      mesh->fill_cbuffer(&ps->mesh_cbuffer());
      ctx->set_cbuffer(vs->mesh_cbuffer(), ps->mesh_cbuffer());

      ctx->draw_indexed(geometry->index_count, 0, 0);
    }

    if (has_resources)
      ctx->unset_shader_resource(0, MAX_TEXTURES, ShaderType::kPixelShader);
  }
}
