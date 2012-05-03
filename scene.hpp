#pragma once
#include "graphics_submit.hpp"

struct TrackedLocation;

struct SubMesh {
	SubMesh();
	~SubMesh();
	RenderKey key;
	MeshRenderData data;
};

struct Mesh {
	Mesh(const std::string &name) : name(name) { }
	void submit(const TrackedLocation &location, uint16 seq_nr, int material_id, GraphicsObjectHandle technique);
	std::string name;
	XMFLOAT4X4 obj_to_world;
	std::vector<SubMesh *> submeshes;
};

struct Camera {
	Camera(const std::string &name) : name(name) {}
	std::string name;
	XMFLOAT3 pos, target, up;
	float roll;
	float aspect_ratio;
	float fov;
	float near_plane, far_plane;
};

struct Light {
  Light(const std::string &name) : name(name) {}
  std::string name;
  XMFLOAT4 pos;
  XMFLOAT4 color;
  float intensity;
};

struct Scene {

	~Scene();

	void submit_meshes(const TrackedLocation &location, uint16 seq_nr, int material_id = -1, GraphicsObjectHandle technique = GraphicsObjectHandle());
	void submit_mesh(const TrackedLocation &location, const char *name, uint16 seq_nr, int material_id = -1, GraphicsObjectHandle technique = GraphicsObjectHandle());

	std::vector<Mesh *> meshes;
	std::vector<Camera *> cameras;
  std::vector<Light *> lights;
};
