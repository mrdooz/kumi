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

struct Camera {
	Camera(const string &name) : name(name) {}
	string name;
	XMFLOAT3 pos, target, up;
	float roll;
};

struct Scene {

	~Scene();

	std::vector<Mesh *> meshes;
	std::vector<Camera *> cameras;
};
