#include "stdafx.h"
#include "scene.hpp"
#include "graphics.hpp"
#include "renderer.hpp"

SubMesh::SubMesh()
{
	key.cmd = RenderKey::kRenderMesh;
}

SubMesh::~SubMesh() {
	key.cmd = RenderKey::kDelete;
	RENDERER.submit_command(FROM_HERE, key, (void *)&data);
}

void Mesh::submit(const TrackedLocation &location, uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
	for (size_t i = 0; i < submeshes.size(); ++i) {
		SubMesh *s = submeshes[i];
		s->key.seq_nr = seq_nr;
		MeshRenderData *p = RENDERER.alloc_command_data<MeshRenderData>();
		memcpy(p, (void *)&s->data, sizeof(MeshRenderData));
		if (material_id != -1)
			p->material_id = (uint16)material_id;
		if (technique.is_valid())
			p->technique = technique;
		RENDERER.submit_command(location, s->key, p);
	}
}

Scene::~Scene() {
	seq_delete(&meshes);
	seq_delete(&cameras);
}

void Scene::submit_meshes(const TrackedLocation &location, uint16 seq_nr, int material_id, 
                          GraphicsObjectHandle technique) {
	for (size_t i = 0; i < meshes.size(); ++i)
		meshes[i]->submit(location, seq_nr, material_id, technique);
}

void Scene::submit_mesh(const TrackedLocation &location, const char *name, uint16 seq_nr, int material_id, 
                        GraphicsObjectHandle technique) {
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
  if (it != _meshes_by_name.end())
    return it->second;

  for (int i = 0; i < (int)meshes.size(); ++i) {
    if (meshes[i]->name == name) {
      _meshes_by_name[name] = meshes[i];
      return meshes[i];
    }
  }
  return nullptr;
}
