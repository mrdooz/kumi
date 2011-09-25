#include "stdafx.h"
#include "scene.hpp"
#include "graphics.hpp"

SubMesh::SubMesh()
{
	key.cmd = RenderKey::kRenderMesh;
}

SubMesh::~SubMesh() {
	key.cmd = RenderKey::kDelete;
	GRAPHICS.submit_command(key, (void *)&data);
}

void Mesh::submit(uint16 seq_nr, int material_id, GraphicsObjectHandle technique) {
	for (size_t i = 0; i < submeshes.size(); ++i) {
		SubMesh *s = submeshes[i];
		s->key.seq_nr = seq_nr;
		MeshRenderData *p = (MeshRenderData *)GRAPHICS.alloc_command_data(sizeof(MeshRenderData));
		memcpy(p, (void *)&s->data, sizeof(MeshRenderData));
		if (material_id != -1)
			p->material_id = (uint16)material_id;
		if (technique.is_valid())
			p->technique = technique;
		GRAPHICS.submit_command(s->key, p);
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
