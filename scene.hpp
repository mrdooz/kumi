#pragma once
#include "graphics.hpp"

struct SubMesh {
	SubMesh();
	~SubMesh();
	RenderKey key;
	MeshRenderData data;
};

struct Mesh {
	Mesh(const string &name) : name(name) { }
	void submit(uint16 seq_nr, uint16 material_id);
	string name;
	XMFLOAT4X4 obj_to_world;
	std::vector<SubMesh *> submeshes;
};

struct Camera {
	Camera(const string &name) : name(name) {}
	string name;
	XMFLOAT3 pos, target, up;
	float roll;
	float aspect_ratio;
	float fov;
	float near_plane, far_plane;
};

struct Scene {

	~Scene();

	void submit_meshes(uint16 seq_nr, uint16 material_id = ~0);

	std::vector<Mesh *> meshes;
	std::vector<Camera *> cameras;
};
