#include "stdafx.h"
#include "kumi_loader.hpp"
#include "material_manager.hpp"
#include "scene.hpp"
#include "dx_utils.hpp"
#include "graphics.hpp"
#include "resource_interface.hpp"
#include "tracked_location.hpp"

#define FILE_VERSION 4

#pragma pack(push, 1)
struct MainHeader {
  int version;
  int material_ofs;
  int mesh_ofs;
  int light_ofs;
  int camera_ofs;
  int animation_ofs;
  int binary_ofs;
  int total_size;
};

namespace BlockId {
  enum Enum {
    kMaterials,
    kMeshes,
    kCameras,
    kLights,
    kAnimation,
  };
}

struct BlockHeader {
  BlockId::Enum id;
  int size;	// size excl header
};
#pragma pack(pop)

static XMFLOAT4 expand_float3(const XMFLOAT3 &v, float w) {
  return XMFLOAT4(v.x, v.y, v.z, w);
}

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

template <class T, class U>
const T& read_and_step(const U **buf) {
  const T &tmp = *(const T *)*buf;
  *buf += sizeof(T);
  return tmp;
}

template <class U>
void read_and_step_raw(const U **buf, void *dst, int len) {
  memcpy(dst, (const void *)*buf, len);
  *buf += len;
}

template <class T, class U>
void read_and_step(const U **buf, T *val) {
  *val = *(const T *)*buf;
  *buf += sizeof(T);
}

bool KumiLoader::load_meshes(const uint8 *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int mesh_count = read_and_step<int>(&buf);
  for (int i = 0; i < mesh_count; ++i) {
    Mesh *mesh = new Mesh(read_and_step<const char *>(&buf));
    mesh->obj_to_world = read_and_step<XMFLOAT4X4>(&buf);
    scene->meshes.push_back(mesh);
    const int sub_meshes = read_and_step<int>(&buf);
    for (int j = 0; j < sub_meshes; ++j) {
      const char *material_name = read_and_step<const char *>(&buf);
      SubMesh *submesh = new SubMesh;
      mesh->submeshes.push_back(submesh);
      submesh->data.material_id = _material_ids[material_name];
      // set the default technique for the material
      GraphicsObjectHandle h = GRAPHICS.find_technique(_default_techniques[material_name].c_str());
      submesh->data.technique = h;
      const int vb_flags = read_and_step<int>(&buf);
      submesh->data.vertex_size = read_and_step<int>(&buf);
      submesh->data.index_format = index_size_to_format(read_and_step<int>(&buf));
      const int *vb = read_and_step<const int*>(&buf);
      const int vb_size = *vb;
      submesh->data.vertex_count = vb_size / submesh->data.vertex_size;
      submesh->data.vb = GRAPHICS.create_static_vertex_buffer(FROM_HERE, vb_size, (const void *)(vb + 1));
      const int *ib = read_and_step<const int*>(&buf);
      const int ib_size = *ib;
      submesh->data.index_count = ib_size / index_format_to_size(submesh->data.index_format);
      submesh->data.ib = GRAPHICS.create_static_index_buffer(FROM_HERE, ib_size, (const void *)(ib + 1));
      submesh->data.mesh_id = (PropertyManager::Id)mesh;
    }
  }

  return true;
}

bool KumiLoader::load_lights(const uint8 *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int count = read_and_step<int>(&buf);
  for (int i = 0; i < count; ++i) {
    Light *light = new Light(read_and_step<const char *>(&buf));
    scene->lights.push_back(light);
    light->pos = expand_float3(read_and_step<XMFLOAT3>(&buf), 0);
    light->color = expand_float3(read_and_step<XMFLOAT3>(&buf), 1);
    light->intensity = read_and_step<float>(&buf);
  }
  return true;
}

bool KumiLoader::load_cameras(const uint8 *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int count = read_and_step<int>(&buf);
  for (int i = 0; i < count; ++i) {
    Camera *camera = new Camera(read_and_step<const char *>(&buf));
    scene->cameras.push_back(camera);
    camera->pos = read_and_step<XMFLOAT3>(&buf);
    camera->target = read_and_step<XMFLOAT3>(&buf);
    camera->up = read_and_step<XMFLOAT3>(&buf);
    camera->roll = read_and_step<float>(&buf);
    camera->aspect_ratio = read_and_step<float>(&buf);
    camera->fov = read_and_step<float>(&buf);
    camera->near_plane = read_and_step<float>(&buf);
    camera->far_plane = read_and_step<float>(&buf);
  }
  return true;
}

template<class T>
void load_animation_inner(const uint8 **buf, map<string, vector<KeyFrame<T>>> *out) {
  const int count = read_and_step<int>(&(*buf));
  for (int i = 0; i < count; ++i) {
    const char *node_name = read_and_step<const char *>(&(*buf));
    auto &keys = (*out)[node_name];
    const int num_frames = read_and_step<int>(&(*buf));
    keys.resize(num_frames);
    read_and_step_raw(&(*buf), &keys[0], num_frames * sizeof(KeyFrame<T>));
  }
}

bool KumiLoader::load_animation(const uint8 *buf, Scene *scene) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  // load different animation types
  load_animation_inner(&buf, &scene->animation_float);
  load_animation_inner(&buf, &scene->animation_vec3);
  load_animation_inner(&buf, &scene->animation_mtx);
  return true;
}


bool KumiLoader::load_materials(const uint8 *buf) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  int count = read_and_step<int>(&buf);
  for (int i = 0; i < count; ++i) {
    string name, technique;
    name = read_and_step<const char *>(&buf);
    technique = read_and_step<const char *>(&buf);
    _default_techniques[name] = technique;
    Material *material = new Material(name);
    int props = read_and_step<int>(&buf);
    for (int j = 0; j < props; ++j) {
      const char *name = read_and_step<const char *>(&buf);
      PropertyType::Enum type = read_and_step<PropertyType::Enum>(&buf);

      switch (type) {

        case PropertyType::kInt:
          material->properties.push_back(MaterialProperty(name, read_and_step<int>(&buf)));
          break;

        case PropertyType::kFloat:
          material->properties.push_back(MaterialProperty(name, read_and_step<float>(&buf)));
          break;

        case PropertyType::kFloat3:
          // note, this is converted to XMFLOAT4 internally
          material->properties.push_back(MaterialProperty(name, read_and_step<XMFLOAT3>(&buf)));
          break;

        case PropertyType::kFloat4:
          material->properties.push_back(MaterialProperty(name, read_and_step<XMFLOAT4>(&buf)));
          break;

      }
    }
    _material_ids[material->name] = MATERIAL_MANAGER.add_material(material, true);
  }

  return true;
}

bool KumiLoader::load(const char *filename, ResourceInterface *resource, Scene **scene) {
  LOG_CONTEXT("%s loading %s", __FUNCTION__, resource->resolve_filename(filename).c_str());
  MainHeader header;
  if (!resource->load_inplace(filename, 0, sizeof(header), (void *)&header))
    return false;
  if (header.version != FILE_VERSION) {
    LOG_ERROR_LN("Incompatible kumi file version: want: %d, got: %d", FILE_VERSION, header.version);
    return false;
  }

  const int scene_data_size = header.binary_ofs;
  vector<uint8> scene_data;
  B_ERR_BOOL(resource->load_partial(filename, 0, scene_data_size, &scene_data));

  const int buffer_data_size = header.total_size - header.binary_ofs;
  vector<uint8> buffer_data;
  B_ERR_BOOL(resource->load_partial(filename, header.binary_ofs, buffer_data_size, &buffer_data));

  // apply the binary fixup
  apply_fixup((int *)&buffer_data[0], &scene_data[0], &buffer_data[0]);

  B_ERR_BOOL(load_materials(&scene_data[0] + header.material_ofs));

  Scene *s = *scene = new Scene;
  B_ERR_BOOL(load_meshes(&scene_data[0] + header.mesh_ofs, s));
  B_ERR_BOOL(load_cameras(&scene_data[0] + header.camera_ofs, s));
  B_ERR_BOOL(load_lights(&scene_data[0] + header.light_ofs, s));
  B_ERR_BOOL(load_animation(&scene_data[0] + header.animation_ofs, s));

  return true;
}
