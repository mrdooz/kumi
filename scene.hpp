#pragma once
#include "graphics.hpp"

struct SubMesh {
	SubMesh();
	~SubMesh();
	RenderKey key;
	MeshRenderData data;
};

struct Mesh {
	Mesh(const string &name) : name(name) {}
	void submit();
	string name;
	std::vector<SubMesh *> submeshes;
};

struct Scene {

	std::vector<Mesh *> meshes;
};
