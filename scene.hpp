#pragma once
#include "graphics.hpp"

struct SubMesh {
	SubMesh(const string &material) : material(material) {}
	string material;
	MeshRenderData data;
};

struct Mesh {
	Mesh(const string &name) : name(name) {}
	string name;
	std::vector<SubMesh *> submeshes;
};

struct Scene {

	std::vector<Mesh *> meshes;
};
