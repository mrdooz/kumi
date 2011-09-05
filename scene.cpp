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

void Mesh::submit(uint16 seq_nr) {
	for (size_t i = 0; i < submeshes.size(); ++i) {
		submeshes[i]->key.seq_nr = seq_nr;
		GRAPHICS.submit_command(submeshes[i]->key, (void *)&submeshes[i]->data);
	}
}

Scene::~Scene() {
	seq_delete(&meshes);
	seq_delete(&cameras);
}

void Scene::submit_meshes(uint16 seq_nr) {
	for (size_t i = 0; i < meshes.size(); ++i)
		meshes[i]->submit(seq_nr);
}
