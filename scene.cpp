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

void Mesh::submit() {
	for (size_t i = 0; i < submeshes.size(); ++i) {
		GRAPHICS.submit_command(submeshes[i]->key, (void *)&submeshes[i]->data);
	}
}
