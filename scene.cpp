#include "stdafx.h"
#include "scene.hpp"
#include "graphics.hpp"
#include "renderer.hpp"
#include "mesh.hpp"

Scene::~Scene() {
  seq_delete(&meshes);
  seq_delete(&cameras);
}

void Scene::submit_meshes(const TrackedLocation &location, uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
  for (auto i = begin(meshes), e = end(meshes); i != e; ++i)
    (*i)->submit(location, seq_nr, material_id, technique);
}

void Scene::submit_mesh(const TrackedLocation &location, const char *name, uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
  static Mesh *prev_mesh = nullptr;
  if (prev_mesh && prev_mesh->name == name) {
    prev_mesh->submit(location, seq_nr, material_id, technique);
  } else {
    for (size_t i = 0; i < meshes.size(); ++i) {
      Mesh *cur = meshes[i];
      if (cur->name == name) {
        cur->submit(location, seq_nr, material_id, technique);
        prev_mesh = cur;
        break;
      }
    }
  }
}

Mesh *Scene::find_mesh_by_name(const std::string &name) {
  auto it = _meshes_by_name.find(name);
  return it != _meshes_by_name.end() ? it->second : nullptr;
}

bool Scene::on_loaded() {
  for (auto it = begin(meshes), e = end(meshes); it != e; ++it) {
    (*it)->prepare_cbuffer();
    _meshes_by_name[(*it)->name] = *it;
  }

  return true;
}


void Scene::update() {
  for (auto i = begin(meshes), e = end(meshes); i != e; ++i)
    (*i)->update();
}
