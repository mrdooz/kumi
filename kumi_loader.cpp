#include "stdafx.h"
#include "kumi_loader.hpp"
#include "material_manager.hpp"
#include "scene.hpp"
#include "dx_utils.hpp"
#include "graphics.hpp"
#include "resource_interface.hpp"
#include "tracked_location.hpp"
#include "mesh.hpp"
#include "file_utils.hpp"
#include "json_utils.hpp"

#define FILE_VERSION 9

#pragma pack(push, 1)
struct MainHeader {
  int version;
  int global_ofs;
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
    *(int *)(p + ofs) += b;
  }
}

template <class T, class U>
const T& read_and_advance(const U **buf) {
  const T &tmp = *(const T *)*buf;
  *buf += sizeof(T);
  return tmp;
}

template <class T, class U>
void read_and_advance(const U **buf, T *val) {
  *val = read_and_advance(buf);
}

template <class U>
void read_and_advance_raw(const U **buf, void *dst, int len) {
  memcpy(dst, (const void *)*buf, len);
  *buf += len;
}


bool KumiLoader::load_meshes(const char *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int mesh_count = read_and_advance<int>(&buf);
  for (int i = 0; i < mesh_count; ++i) {

    Mesh *mesh = new Mesh(read_and_advance<const char *>(&buf));
    mesh->obj_to_world = read_and_advance<XMFLOAT4X4>(&buf);
    scene->meshes.push_back(mesh);
    const int sub_meshes = read_and_advance<int>(&buf);

    for (int j = 0; j < sub_meshes; ++j) {
      SubMesh *submesh = new SubMesh(mesh);
      submesh->name = read_and_advance<const char *>(&buf);
      const char *material_name = read_and_advance<const char *>(&buf);
      mesh->submeshes.push_back(submesh);
      // check if we have a material/technique override
      auto it = _material_overrides.find(submesh->name);
      if (it != end(_material_overrides)) {
        auto &technique = it->second.first;
        auto &material = it->second.second;
        submesh->material_id = _material_name_to_id[material];
        //submesh->render_data.cur_technique = GRAPHICS.find_technique(technique.c_str());
      } else {
        // set the default technique for the material
        submesh->material_id = _material_name_to_id[material_name];
        //submesh->render_data.cur_technique = GRAPHICS.find_technique(_technique_for_material[material_name].c_str());
      }

      const int vb_flags = read_and_advance<int>(&buf);
      submesh->geometry.vertex_size = read_and_advance<int>(&buf);
      submesh->geometry.index_format = index_size_to_format(read_and_advance<int>(&buf));
      const int *vb = read_and_advance<const int*>(&buf);
      const int vb_size = *vb;
      submesh->geometry.vertex_count = vb_size / submesh->geometry.vertex_size;
      submesh->geometry.vb = GRAPHICS.create_static_vertex_buffer(FROM_HERE, vb_size, (const void *)(vb + 1));
      const int *ib = read_and_advance<const int*>(&buf);
      const int ib_size = *ib;
      submesh->geometry.index_count = ib_size / index_format_to_size(submesh->geometry.index_format);
      submesh->geometry.ib = GRAPHICS.create_static_index_buffer(FROM_HERE, ib_size, (const void *)(ib + 1));
    }
  }

  return true;
}

bool KumiLoader::load_lights(const char *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int count = read_and_advance<int>(&buf);
  for (int i = 0; i < count; ++i) {
    Light *light = new Light(read_and_advance<const char *>(&buf));
    scene->lights.push_back(light);
    light->pos = expand_float3(read_and_advance<XMFLOAT3>(&buf), 0);
    light->color = expand_float3(read_and_advance<XMFLOAT3>(&buf), 1);
    light->intensity = read_and_advance<float>(&buf);

    light->use_far_attenuation = read_and_advance<bool>(&buf);
    light->far_attenuation_start = read_and_advance<float>(&buf);
    light->far_attenuation_end = read_and_advance<float>(&buf);
  }
  return true;
}

bool KumiLoader::load_cameras(const char *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int count = read_and_advance<int>(&buf);
  for (int i = 0; i < count; ++i) {
    Camera *camera = new Camera(read_and_advance<const char *>(&buf));
    scene->cameras.push_back(camera);
    camera->pos = read_and_advance<XMFLOAT3>(&buf);
    camera->target = read_and_advance<XMFLOAT3>(&buf);
    camera->up = read_and_advance<XMFLOAT3>(&buf);
    camera->roll = read_and_advance<float>(&buf);
    camera->aspect_ratio = read_and_advance<float>(&buf);
    camera->fov_x = XMConvertToRadians(read_and_advance<float>(&buf));
    camera->fov_y = XMConvertToRadians(read_and_advance<float>(&buf));
    camera->near_plane = read_and_advance<float>(&buf);
    camera->far_plane = read_and_advance<float>(&buf);
  }
  return true;
}

template<class T>
void load_animation_inner(const char **buf, map<string, vector<KeyFrame<T>>> *out) {
  const int count = read_and_advance<int>(&(*buf));
  for (int i = 0; i < count; ++i) {
    const char *node_name = read_and_advance<const char *>(&(*buf));
    auto &keys = (*out)[node_name];
    const int num_frames = read_and_advance<int>(&(*buf));
    keys.resize(num_frames);
    read_and_advance_raw(&(*buf), &keys[0], num_frames * sizeof(KeyFrame<T>));
  }
}

bool KumiLoader::load_animation(const char *buf, Scene *scene) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  // load different animation types
  load_animation_inner(&buf, &scene->animation_float);
  load_animation_inner(&buf, &scene->animation_vec3);
  load_animation_inner(&buf, &scene->animation_mtx);
  return true;
}

bool KumiLoader::load_globals(const char *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);
  scene->ambient = read_and_advance<XMFLOAT4>(&buf);
  return true;
}

bool KumiLoader::load_materials(const char *buf, Scene *scene) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  int num_materials = read_and_advance<int>(&buf);
  for (int i = 0; i < num_materials; ++i) {
    string name = read_and_advance<const char *>(&buf);
    string technique = read_and_advance<const char *>(&buf);
    _technique_for_material[name] = technique;
    Material *material = new Material(name);
    _material_name_to_id[material->name()] = MATERIAL_MANAGER.add_material(material, true);

    int num_props = read_and_advance<int>(&buf);
    for (int j = 0; j < num_props; ++j) {
      const char *name = read_and_advance<const char *>(&buf);
      string filename(read_and_advance<const char *>(&buf));
      GraphicsObjectHandle resource;
      if (!filename.empty()) {
        D3DX11_IMAGE_INFO info;
        // if the path isn't absolute, append the path of the file we're loading
        if (!Path::is_absolute(filename)) {
          filename = Path::get_path(_filename) + filename;
        }
        resource = GRAPHICS.load_texture(filename.c_str(), name, true, &info);
        material->add_flag(GRAPHICS.get_shader_flag("DIFFUSE_TEXTURE"));
        if (!resource.is_valid())
          LOG_WARNING_LN("Unable to load texture: %s", filename.c_str());
      }

      if (!strcmp(name, "Diffuse")) {
        material->add_property("HasDiffuseMap", PropertyType::kInt, filename.empty() ? 0 : 1);
      }

      PropertyType::Enum type = read_and_advance<PropertyType::Enum>(&buf);
      Material::Property *property = nullptr;

      switch (type) {
/*
        case PropertyType::kInt:
          material->properties.push_back(MaterialProperty(name, read_and_advance<int>(&buf)));
          break;
*/
        case PropertyType::kFloat:
          property = material->add_property(name, type, read_and_advance<float>(&buf));
          break;

        case PropertyType::kFloat3: {
          XMFLOAT3 tmp = read_and_advance<XMFLOAT3>(&buf);
          property = material->add_property(name, type, XMFLOAT4(tmp.x, tmp.y, tmp.z, 0));
          break;
        }

        case PropertyType::kFloat4:
        case PropertyType::kColor:
          property = material->add_property(name, type, read_and_advance<XMFLOAT4>(&buf));
          break;

        default:
          LOG_ERROR_LN("Unknown property type");
          break;
      }
      if (property)
        property->resource = resource;
    }
    scene->materials.push_back(material);
  }

  return true;
}

bool KumiLoader::load(const char *filename, const char *material_override, ResourceInterface *resource, Scene **scene) {
  _filename = resource->resolve_filename(filename);
  LOG_CONTEXT("%s loading %s", __FUNCTION__, _filename.c_str());

  if (material_override) {
    vector<char> buf;
    load_file(material_override, &buf);
    JsonValue::JsonValuePtr root = parse_json(buf.data(), buf.data() + buf.size());

    if (root->has_key("material_connections")) {
      auto &connections = root->get("material_connections");
      for (int i = 0; i < connections->count(); ++i) {
        auto &cur = connections->get(i);
        auto &submesh = cur->get("submesh")->to_string();
        auto &technique = cur->get("technique")->to_string();
        auto &material = cur->get("material")->to_string();
        _material_overrides[submesh] = make_pair(technique, material);
      }
    }

    if (root->has_key("materials")) {
      auto &materials = root->get("materials");
      for (int i = 0; i < materials->count(); ++i) {
        auto &cur = materials->get(i);
        auto &name = cur->get("name")->to_string();
        auto &properties = cur->get("properties");
        if (!properties->is_null()) {
          for (int j = 0; j < properties->count(); ++j) {
            auto &property = properties->get(j);
            auto &name = property->get("name")->to_string();
            auto &type = property->get("type")->to_string();
            auto &value = property->get("value");
            XMFLOAT4 v;
            if (type == "float") {
              v.x = (float)value->get("x")->to_number();
            } else if (type == "color") {
              v.x = (float)value->get("r")->to_number();
              v.y = (float)value->get("g")->to_number();
              v.z = (float)value->get("b")->to_number();
              v.w = (float)value->get("a")->to_number();
            }
          }
        }
      }
    }
  }

  MainHeader header;
  if (!resource->load_inplace(filename, 0, sizeof(header), (void *)&header))
    return false;
  if (header.version != FILE_VERSION) {
    LOG_ERROR_LN("Incompatible kumi file version: want: %d, got: %d", FILE_VERSION, header.version);
    return false;
  }

  const int scene_data_size = header.binary_ofs;
  vector<char> scene_data;
  B_ERR_BOOL(resource->load_partial(filename, 0, scene_data_size, &scene_data));

  const int buffer_data_size = header.total_size - header.binary_ofs;
  vector<char> buffer_data;
  B_ERR_BOOL(resource->load_partial(filename, header.binary_ofs, buffer_data_size, &buffer_data));

  // apply the binary fixup
  apply_fixup((int *)&buffer_data[0], &scene_data[0], &buffer_data[0]);

  Scene *s = *scene = new Scene;

  B_ERR_BOOL(load_globals(&scene_data[0] + header.global_ofs, s));
  B_ERR_BOOL(load_materials(&scene_data[0] + header.material_ofs, s));
  B_ERR_BOOL(load_meshes(&scene_data[0] + header.mesh_ofs, s));
  B_ERR_BOOL(load_cameras(&scene_data[0] + header.camera_ofs, s));
  B_ERR_BOOL(load_lights(&scene_data[0] + header.light_ofs, s));
  B_ERR_BOOL(load_animation(&scene_data[0] + header.animation_ofs, s));

  B_ERR_BOOL(s->on_loaded());

  return true;
}
