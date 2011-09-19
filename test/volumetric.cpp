#include "stdafx.h"
#include "volumetric.hpp"
#include "../kumi_loader.hpp"
#include "../app.hpp"
#include "../scene.hpp"
#include "../material_manager.hpp"
#include "../technique.hpp"

bool VolumetricEffect::init() {

	KumiLoader loader;
	loader.load("meshes\\torus.kumi", &_scene);

	for (size_t i = 0; i < _scene->meshes.size(); ++i) {
		XMMATRIX mtx = XMMatrixTranspose(XMLoadFloat4x4(&_scene->meshes[i]->obj_to_world));
		XMFLOAT4X4 tmp;
		XMStoreFloat4x4(&tmp, mtx);
		PROPERTY_MANAGER.set_mesh_property((PropertyManager::Id)_scene->meshes[i], "world", tmp);
	}

	B_ERR_BOOL(GRAPHICS.load_techniques("effects/volumetric.tec", NULL));

	material_occlude = GRAPHICS.find_technique("volumetric_occluder")->material_id("black");
	material_shaft = GRAPHICS.find_technique("volumetric_shaft")->material_id("default");

	int w, h;
	GRAPHICS.size(&w, &h);
	_occluder_rt = GRAPHICS.create_render_target(w, h, true, "volumetric_occluder");
	_shaft_rt = GRAPHICS.create_render_target(w, h, true, "volumetric_shaft");
	B_ERR_BOOL(_occluder_rt.is_valid() && _shaft_rt.is_valid());

	return true;
}

bool VolumetricEffect::update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) {

	if (_scene && !_scene->cameras.empty()) {
		Camera *camera = _scene->cameras[0];
		XMMATRIX lookat = XMMatrixTranspose(XMMatrixLookAtLH(
			XMVectorSet(camera->pos.x,camera->pos.y,camera->pos.z,0),
			XMVectorSet(camera->target.x,camera->target.y,camera->target.z,0),
			XMVectorSet(camera->up.x,camera->up.y,camera->up.z,0)));

		XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(
			XMConvertToRadians(camera->fov),
			camera->aspect_ratio,
			camera->near_plane, camera->far_plane));

		XMFLOAT4X4 lookat_tmp, proj_tmp;
		XMStoreFloat4x4(&lookat_tmp, lookat);
		XMStoreFloat4x4(&proj_tmp, proj);

		PROPERTY_MANAGER.set_system_property("view", lookat_tmp);
		PROPERTY_MANAGER.set_system_property("proj", proj_tmp);
	}

	return true;
}

bool VolumetricEffect::render() {

	static XMFLOAT4 clear_white(1, 1, 1, 1);
	static XMFLOAT4 clear_black(1, 1, 1, 1);

	if (_scene) {

		RenderKey key;
		key.data = 0;
		key.cmd = RenderKey::kSetRenderTarget;
		key.handle = _occluder_rt;
		key.seq_nr = GRAPHICS.next_seq_nr();
		GRAPHICS.submit_command(key, (void *)&clear_white);

		// Render the occluders

		// Use the same sequence number for all the meshes to sort by technique
		_scene->submit_meshes(GRAPHICS.next_seq_nr(), material_occlude);

		// Render the light streaks
		key.handle = _shaft_rt;
		key.seq_nr = GRAPHICS.next_seq_nr();
		GRAPHICS.submit_command(key, (void *)&clear_black);

		GRAPHICS.find_technique("volumetric_shaft")->submit();

		// Add it together
		key.handle = GRAPHICS.default_rt_handle();
		key.seq_nr = GRAPHICS.next_seq_nr();
		GRAPHICS.submit_command(key, NULL);
	}

	return true;
}

bool VolumetricEffect::close() {
	return true;
}
