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

void Mesh::submit(uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
	for (size_t i = 0; i < submeshes.size(); ++i) {
		SubMesh *s = submeshes[i];
		s->key.seq_nr = seq_nr;
		MeshRenderData *p = (MeshRenderData *)RENDERER.alloc_command_data(sizeof(MeshRenderData));
		memcpy(p, (void *)&s->data, sizeof(MeshRenderData));
		if (material_id != -1)
			p->material_id = (uint16)material_id;
		if (technique.is_valid())
			p->technique = technique;
		RENDERER.submit_command(FROM_HERE, s->key, p);
	}
}

Scene::~Scene() {
	seq_delete(&meshes);
	seq_delete(&cameras);
}

void Scene::submit_meshes(uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
	for (size_t i = 0; i < meshes.size(); ++i)
		meshes[i]->submit(seq_nr, material_id, technique);
}

void Scene::submit_mesh(const char *name, uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
	static Mesh *prev_mesh = nullptr;
	if (prev_mesh && prev_mesh->name == name) {
		prev_mesh->submit(seq_nr, material_id, technique);
	} else {
		for (size_t i = 0; i < meshes.size(); ++i) {
			Mesh *cur = meshes[i];
			if (cur->name == name) {
				cur->submit(seq_nr, material_id, technique);
				prev_mesh = cur;
				break;
			}
		}
	}
}
