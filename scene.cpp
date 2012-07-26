#include "stdafx.h"
#include "scene.hpp"
#include "graphics.hpp"
#include "mesh.hpp"
#include "material.hpp"
#include "animation_manager.hpp"

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
