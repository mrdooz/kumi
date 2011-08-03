#include "stdafx.h"
#include "kumi_loader.hpp"
#include "io.hpp"
#include "material_manager.hpp"
#include "scene.hpp"

#pragma pack(push, 1)
struct MainHeader {
	int material_ofs;
	int mesh_ofs;
	int string_ofs;
	int binary_ofs;
	int total_size;
};

namespace BlockId {
	enum Enum {
		kMaterials,
		kMeshes
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
		scene->meshes.push_back(mesh);
		const int sub_meshes = step_read<int>(&buf);
		for (int j = 0; j < sub_meshes; ++j) {
			SubMesh *submesh = new SubMesh(step_read<const char *>(&buf));
			mesh->submeshes.push_back(submesh);
			const int vb_flags = step_read<int>(&buf);
			const int vb_elem_size = step_read<int>(&buf);
			const int ib_elem_size = step_read<int>(&buf);
			const char *vb = step_read<const char*>(&buf);
			const int vb_size = *(int*)vb;
			submesh->data.vb = GRAPHICS.create_static_vertex_buffer(vb_size, (const void *)(vb + 4));
			const char *ib = step_read<const char*>(&buf);
			const int ib_size = *(int*)ib;
			submesh->data.ib = GRAPHICS.create_static_index_buffer(ib_size, (const void *)(ib + 4));
		}
	}

	return true;
}

bool KumiLoader::load_materials(const char *buf) {

	BlockHeader *header = (BlockHeader *)buf;
	buf += sizeof(BlockHeader);

	int count = step_read<int>(&buf);
	for (int i = 0; i < count; ++i) {
		Material material(step_read<const char *>(&buf));
		int props = step_read<int>(&buf);
		for (int j = 0; j < props; ++j) {
			const char *name = step_read<const char *>(&buf);
			MaterialProperty::Type type = step_read<MaterialProperty::Type>(&buf);
			switch (type) {
			case MaterialProperty::kInt:
				material.properties.push_back(MaterialProperty(name, step_read<int>(&buf)));
				break;
			case MaterialProperty::kFloat:
				material.properties.push_back(MaterialProperty(name, step_read<float>(&buf)));
				break;

			case MaterialProperty::kFloat4:
				material.properties.push_back(MaterialProperty(name, step_read<XMFLOAT4>(&buf)));
				break;
			}
		}
		MATERIAL_MANAGER.add_material(material);
	}

	return true;
}

bool KumiLoader::load(const char *filename, Io *io, Scene **scene) {

	MainHeader header;
	MainHeader *p = &header;
	B_ERR_BOOL(io->load_partial(filename, 0, sizeof(header), (void **)&p));

	const int scene_data_size = header.binary_ofs;
	const char *scene_data = new char[scene_data_size];
	B_ERR_BOOL(io->load_partial(filename, 0, scene_data_size, (void **)&scene_data));

	const int buffer_data_size = header.total_size - header.binary_ofs;
	char *buffer_data = new char[buffer_data_size];
	B_ERR_BOOL(io->load_partial(filename, header.binary_ofs, buffer_data_size, (void **)&buffer_data));

	// apply the string fix-up
	apply_fixup((int *)(scene_data + header.string_ofs), scene_data, scene_data);

	// apply the binary fixup
	apply_fixup((int *)buffer_data, scene_data, buffer_data);

	B_ERR_BOOL(load_materials(scene_data + header.material_ofs));

	Scene *s = *scene = new Scene;
	B_ERR_BOOL(load_meshes(scene_data + header.mesh_ofs, s));

	return true;
}
