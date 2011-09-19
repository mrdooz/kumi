#include "stdafx.h"
#include "kumi_loader.hpp"
#include "material_manager.hpp"
#include "scene.hpp"
#include "dx_utils.hpp"
#include "resource_manager.hpp"

#pragma pack(push, 1)
struct MainHeader {
	int material_ofs;
	int mesh_ofs;
	int light_ofs;
	int camera_ofs;
	int string_ofs;
	int binary_ofs;
	int total_size;
};

namespace BlockId {
	enum Enum {
		kMaterials,
		kMeshes,
		kCameras,
		kLights,
	};
}

struct BlockHeader {
	BlockId::Enum id;
	int size;	// size excl header
};
#pragma pack(pop)


static void apply_fixup(int *data, const void *ptr_base, const void *data_base) {
	// the data is stored as a int count followed by #count int offsets
	int b = (int)data_base;
	int p = (int)ptr_base;
	const int count = *data++;
	for (int i = 0; i < count; ++i) {
		int ofs = *data++;
		*(int *)(p + ofs) += (int)b;
	}
}

template <class T>
const T& step_read(const char **buf) {
	const T &tmp = *(const T *)*buf;
	*buf += sizeof(T);
	return tmp;
}

bool KumiLoader::load_meshes(const char *buf, Scene *scene) {
	BlockHeader *header = (BlockHeader *)buf;
	buf += sizeof(BlockHeader);

	const int mesh_count = step_read<int>(&buf);
	for (int i = 0; i < mesh_count; ++i) {
		Mesh *mesh = new Mesh(step_read<const char *>(&buf));
		mesh->obj_to_world = step_read<XMFLOAT4X4>(&buf);
		scene->meshes.push_back(mesh);
		const int sub_meshes = step_read<int>(&buf);
		for (int j = 0; j < sub_meshes; ++j) {
			const char *material_name = step_read<const char *>(&buf);
			SubMesh *submesh = new SubMesh;
			submesh->data.material_id = _material_ids[material_name];
			mesh->submeshes.push_back(submesh);
			const int vb_flags = step_read<int>(&buf);
			submesh->data.vertex_size = step_read<int>(&buf);
			submesh->data.index_format = index_size_to_format(step_read<int>(&buf));
			const int *vb = step_read<const int*>(&buf);
			const int vb_size = *vb;
			submesh->data.vertex_count = vb_size / submesh->data.vertex_size;
			submesh->data.vb = GRAPHICS.create_static_vertex_buffer(vb_size, (const void *)(vb + 1));
			const int *ib = step_read<const int*>(&buf);
			const int ib_size = *ib;
			submesh->data.index_count = ib_size / index_format_to_size(submesh->data.index_format);
			submesh->data.ib = GRAPHICS.create_static_index_buffer(ib_size, (const void *)(ib + 1));
			submesh->data.mesh_id = (PropertyManager::Id)mesh;
		}
	}

	return true;
}

bool KumiLoader::load_cameras(const char *buf, Scene *scene) {
	BlockHeader *header = (BlockHeader *)buf;
	buf += sizeof(BlockHeader);

	const int count = step_read<int>(&buf);
	for (int i = 0; i < count; ++i) {
		Camera *camera = new Camera(step_read<const char *>(&buf));
		scene->cameras.push_back(camera);
		camera->pos = step_read<XMFLOAT3>(&buf);
		camera->target = step_read<XMFLOAT3>(&buf);
		camera->up = step_read<XMFLOAT3>(&buf);
		camera->roll = step_read<float>(&buf);
		camera->aspect_ratio = step_read<float>(&buf);
		camera->fov = step_read<float>(&buf);
		camera->near_plane = step_read<float>(&buf);
		camera->far_plane = step_read<float>(&buf);
	}

	return true;
}


bool KumiLoader::load_materials(const char *buf) {

	BlockHeader *header = (BlockHeader *)buf;
	buf += sizeof(BlockHeader);

	int count = step_read<int>(&buf);
	for (int i = 0; i < count; ++i) {
		string name, technique;
		name = step_read<const char *>(&buf);
		technique = step_read<const char *>(&buf);
		Material material(technique, name);
		int props = step_read<int>(&buf);
		for (int j = 0; j < props; ++j) {
			const char *name = step_read<const char *>(&buf);
			PropertyType::Enum type = step_read<PropertyType::Enum>(&buf);
			switch (type) {
			case PropertyType::kInt:
				material.properties.push_back(MaterialProperty(name, step_read<int>(&buf)));
				break;
			case PropertyType::kFloat:
				material.properties.push_back(MaterialProperty(name, step_read<float>(&buf)));
				break;

			case PropertyType::kFloat4:
				material.properties.push_back(MaterialProperty(name, step_read<XMFLOAT4>(&buf)));
				break;
			}
		}
		_material_ids[material.name] = MATERIAL_MANAGER.add_material(material);
	}

	return true;
}

bool KumiLoader::load(const char *filename, Scene **scene) {

	MainHeader header;
	MainHeader *p = &header;
	B_ERR_BOOL(RESOURCE_MANAGER.load_partial(filename, 0, sizeof(header), (void **)&p));

	const int scene_data_size = header.binary_ofs;
	const char *scene_data = new char[scene_data_size];
	B_ERR_BOOL(RESOURCE_MANAGER.load_partial(filename, 0, scene_data_size, (void **)&scene_data));

	const int buffer_data_size = header.total_size - header.binary_ofs;
	char *buffer_data = new char[buffer_data_size];
	B_ERR_BOOL(RESOURCE_MANAGER.load_partial(filename, header.binary_ofs, buffer_data_size, (void **)&buffer_data));

	// apply the string fix-up
	apply_fixup((int *)(scene_data + header.string_ofs), scene_data, scene_data);

	// apply the binary fixup
	apply_fixup((int *)buffer_data, scene_data, buffer_data);

	B_ERR_BOOL(load_materials(scene_data + header.material_ofs));

	Scene *s = *scene = new Scene;
	B_ERR_BOOL(load_meshes(scene_data + header.mesh_ofs, s));
	B_ERR_BOOL(load_cameras(scene_data + header.camera_ofs, s));

	return true;
}
