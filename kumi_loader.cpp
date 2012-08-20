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
#include "animation_manager.hpp"
#include "bit_utils.hpp"

using namespace std;

#define FILE_VERSION 13

#pragma pack(push, 1)
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

static void apply_fixup(const void *data, const void *ptr_base, const void *data_base) {
  struct FixupData {
    int count;
    int offsets[0];
  } *fixup = (FixupData *)data;
  int b = (int)data_base;
  int p = (int)ptr_base;
  for (int i = 0; i < fixup->count; ++i) {
    int ofs = fixup->offsets[i];
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

static enum VertexFlags {
  kPos     = 1 << 0,
  kNormal  = 1 << 1,
  kTex0    = 1 << 2,
  kTex1    = 1 << 3,
};

uint32 quantize(float value, int num_bits) {
  // assume value is in the [-1..1] range
  uint32 scale = (1 << (num_bits-1)) - 1;
  uint32 v = (uint32)(scale * fabs(value));
  if (value < 0)
    v = set_bit(v, num_bits - 1);
  return v;
}

float dequant(uint32 value, int num_bits) {
  uint32 uscale = (1 << (num_bits-1)) - 1;
  bool neg = is_bit_set(value, num_bits-1);
  float f = clear_bit(value, num_bits-1) / (float)uscale;
  return (neg ? -1 : 1) * f;
}

void KumiLoader::decompress_ib(BitReader *reader, int num_indices, int index_size, vector<char> *out) {

  out->resize(num_indices * index_size);

  if (index_size == 2) {
    uint16 *buf = (uint16 *)out->data();
    int prev = reader->read_varint();
    prev = zigzag_decode(prev);
    *buf++ = prev;
    for (int i = 1; i < num_indices; ++i) {
      int delta = reader->read_varint();
      delta = zigzag_decode(delta);
      int cur = prev + delta;
      *buf++ = cur;
      prev = cur;
    }

  } else {
    int32 *buf = (int32 *)out->data();
    int prev = reader->read_varint();
    prev = zigzag_decode(prev);
    *buf++ = prev;
    for (int i = 1; i < num_indices; ++i) {
      int delta = reader->read_varint();
      delta = zigzag_decode(delta);
      int cur = prev + delta;
      *buf++ = cur;
      prev = cur;
    }
  }

}

void KumiLoader::decompress_vb(Mesh *mesh, BitReader *reader, int num_verts, int vertex_size, int vb_flags, vector<char> *out) {

  out->resize(num_verts * vertex_size);
  char *buf = out->data();

  int pos_bits = _header.position_bits;
  int normal_bits = _header.normal_bits;
  int tex_bits = _header.texcoord_bits;

  for (int i = 0; i < num_verts; ++i) {

    float x = mesh->_center.x + mesh->_extents.x * dequant(reader->read(pos_bits), pos_bits);
    float y = mesh->_center.y + mesh->_extents.y * dequant(reader->read(pos_bits), pos_bits);
    float z = mesh->_center.z + mesh->_extents.z * dequant(reader->read(pos_bits), pos_bits);
    *(XMFLOAT3 *)buf = XMFLOAT3(x, y, z);
    buf += sizeof(XMFLOAT3);

    XMFLOAT3 n;
    n.x = dequant(reader->read(normal_bits), normal_bits);
    n.y = dequant(reader->read(normal_bits), normal_bits);
    bool neg_z = !!reader->read(1);
    n.z = (neg_z ? -1 : 1) * sqrtf(1 - n.x * n.x - n.y * n.y);
    *(XMFLOAT3 *)buf = normalize(n);
    buf += sizeof(XMFLOAT3);

    if (vb_flags & kTex0) {
      XMFLOAT2 t;
      t.x = dequant(reader->read(tex_bits), tex_bits);
      t.y = dequant(reader->read(tex_bits), tex_bits);
      *(XMFLOAT2 *)buf = t;
      buf += sizeof(XMFLOAT2);
    }
  }
}

bool KumiLoader::load_meshes(const char *buf, Scene *scene) {
  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);

  const int mesh_count = read_and_advance<int>(&buf);
  for (int i = 0; i < mesh_count; ++i) {

    Mesh *mesh = new Mesh(read_and_advance<const char *>(&buf));
    mesh->_pos = read_and_advance<XMFLOAT3>(&buf);
    mesh->_rot = read_and_advance<XMFLOAT4>(&buf);
    mesh->_scale = read_and_advance<XMFLOAT3>(&buf);
    mesh->_obj_to_world = compose_transform(mesh->_pos, mesh->_rot, mesh->_scale, true);

    mesh->_center = read_and_advance<XMFLOAT3>(&buf);
    mesh->_extents = read_and_advance<XMFLOAT3>(&buf);
    mesh->_is_static = read_and_advance<bool>(&buf);

    scene->meshes.push_back(mesh);
    const int sub_meshes = read_and_advance<int>(&buf);

    for (int j = 0; j < sub_meshes; ++j) {
      SubMesh *submesh = new SubMesh(mesh);
      submesh->_name = read_and_advance<const char *>(&buf);
      const char *material_name = read_and_advance<const char *>(&buf);
      mesh->_submeshes.push_back(submesh);
      // check if we have a material/technique override
      auto it = _material_overrides.find(submesh->name());
      if (it != end(_material_overrides)) {
        auto &technique = it->second.first;
        auto &material = it->second.second;
        submesh->_material_id = _material_name_to_id[material];
        //submesh->render_data.cur_technique = GRAPHICS.find_technique(technique.c_str());
      } else {
        // set the default technique for the material
        submesh->_material_id = _material_name_to_id[material_name];
        //submesh->render_data.cur_technique = GRAPHICS.find_technique(_technique_for_material[material_name].c_str());
      }

      const int vb_flags = read_and_advance<int>(&buf);
      int vertex_size = read_and_advance<int>(&buf);
      submesh->_geometry.vertex_size = vertex_size;
      int index_size = read_and_advance<int>(&buf);
      submesh->_geometry.index_format = index_size_to_format(index_size);

      int num_verts = read_and_advance<int>(&buf);
      int num_indices = read_and_advance<int>(&buf);

      const int *vb = read_and_advance<const int*>(&buf);
      const int vb_size = *vb;
      vector<char> decompressed_vb;
      BitReader vb_reader((const uint8 *)(vb + 1), vb_size * 8);
      decompress_vb(mesh, &vb_reader, num_verts, vertex_size, vb_flags, &decompressed_vb);

      submesh->_geometry.vertex_count = num_verts;
      submesh->_geometry.vb = GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_VERTEX_BUFFER, decompressed_vb.size(), false, decompressed_vb.data());


      const int *ib = read_and_advance<const int*>(&buf);
      const int ib_size = *ib;
      vector<char> decompressed_ib;
      BitReader ib_reader((const uint8 *)(ib + 1), ib_size * 8);
      decompress_ib(&ib_reader, num_indices, index_size, &decompressed_ib);

      submesh->_geometry.index_count = num_indices;
      submesh->_geometry.ib = GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_INDEX_BUFFER, decompressed_ib.size(), false, decompressed_ib.data());
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
    light->is_static = read_and_advance<bool>(&buf);
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
    camera->is_static = read_and_advance<bool>(&buf);
  }
  return true;
}

static XMFLOAT3 read_float3(BitReader *reader) {
  uint32 x = reader->read(32);
  uint32 y = reader->read(32);
  uint32 z = reader->read(32);
  return XMFLOAT3(*(float *)&x, *(float *)&y, *(float *)&z);
}

static XMFLOAT4 read_float4(BitReader *reader) {
  uint32 x = reader->read(32);
  uint32 y = reader->read(32);
  uint32 z = reader->read(32);
  uint32 w = reader->read(32);
  return XMFLOAT4(*(float *)&x, *(float *)&y, *(float *)&z, *(float *)&w);
}

static float read_float(BitReader *reader) {
  uint32 v = reader->read(32);
  return *(float *)&v;
}

bool KumiLoader::load_animation(const char *buf, Scene *scene) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);
  const char *buf_end = buf + header->size;

  int anim_bits = _header.animation_bits;

  while (buf < buf_end) {

    string node_name = read_and_advance<const char *>(&buf);
    {
      // pos
      const int *frames = read_and_advance<const int*>(&buf);
      const int frames_size = *frames;
      BitReader reader((const uint8 *)(frames + 1), frames_size * 8);

      if (int num_pos_keys = reader.read_varint()) {
        XMFLOAT3 center = read_float3(&reader);
        XMFLOAT3 extents = read_float3(&reader);

        KeyFrameVec3 *keyframes = (KeyFrameVec3 *)ANIMATION_MANAGER.alloc_anim(node_name, AnimationManager::kAnimPos, num_pos_keys);
        for (int i = 0; i < num_pos_keys; ++i) {
          keyframes[i].time = read_float(&reader);

          float x = center.x + extents.x * dequant(reader.read(anim_bits), anim_bits);
          float y = center.y + extents.y * dequant(reader.read(anim_bits), anim_bits);
          float z = center.z + extents.z * dequant(reader.read(anim_bits), anim_bits);
          keyframes[i].value = XMFLOAT3(x, y, z);
        }
      }
    }

    {
      // rot
      const int *frames = read_and_advance<const int*>(&buf);
      const int frames_size = *frames;
      BitReader reader((const uint8 *)(frames + 1), frames_size * 8);

      if (int num_rot_keys = reader.read_varint()) {
        XMFLOAT4 center = read_float4(&reader);
        XMFLOAT4 extents = read_float4(&reader);

        KeyFrameVec4 *keyframes = (KeyFrameVec4 *)ANIMATION_MANAGER.alloc_anim(node_name, AnimationManager::kAnimRot, num_rot_keys);
        for (int i = 0; i < num_rot_keys; ++i) {
          keyframes[i].time = read_float(&reader);

          float x = center.x + extents.x * dequant(reader.read(anim_bits), anim_bits);
          float y = center.y + extents.y * dequant(reader.read(anim_bits), anim_bits);
          float z = center.z + extents.z * dequant(reader.read(anim_bits), anim_bits);
          float w = center.w + extents.w * dequant(reader.read(anim_bits), anim_bits);
          keyframes[i].value = XMFLOAT4(x, y, z, w);
        }
      }
    }

    {
      // scale
      const int *frames = read_and_advance<const int*>(&buf);
      const int frames_size = *frames;
      BitReader reader((const uint8 *)(frames + 1), frames_size * 8);

      if (int num_scale_keys = reader.read_varint()) {
        XMFLOAT3 center = read_float3(&reader);
        XMFLOAT3 extents = read_float3(&reader);

        KeyFrameVec3 *keyframes = (KeyFrameVec3 *)ANIMATION_MANAGER.alloc_anim(node_name, AnimationManager::kAnimScale, num_scale_keys);
        for (int i = 0; i < num_scale_keys; ++i) {
          keyframes[i].time = read_float(&reader);

          float x = center.x + extents.x * dequant(reader.read(anim_bits), anim_bits);
          float y = center.y + extents.y * dequant(reader.read(anim_bits), anim_bits);
          float z = center.z + extents.z * dequant(reader.read(anim_bits), anim_bits);
          keyframes[i].value = XMFLOAT3(x, y, z);
        }
      }
    }
  }

  ANIMATION_MANAGER.on_loaded();

  return true;
}

bool KumiLoader::load_animation2(const char *buf, Scene *scene) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);
  const char *buf_end = buf + header->size;

  int anim_bits = _header.animation_bits;

  while (buf < buf_end) {

    string node_name = read_and_advance<const char *>(&buf);

    if (int num_control_pts = read_and_advance<int>(&buf)) {
      auto control_pts = ANIMATION_MANAGER.alloc_anim2(node_name, AnimationManager::kAnimPos, num_control_pts);
      read_and_advance_raw(&buf, control_pts, num_control_pts * sizeof(XMFLOAT3));
    }

    if (int num_control_pts = read_and_advance<int>(&buf)) {
      auto control_pts = ANIMATION_MANAGER.alloc_anim2(node_name, AnimationManager::kAnimRot, num_control_pts);
      read_and_advance_raw(&buf, control_pts, num_control_pts * sizeof(XMFLOAT4));
    }

    if (int num_control_pts = read_and_advance<int>(&buf)) {
      auto control_pts = ANIMATION_MANAGER.alloc_anim2(node_name, AnimationManager::kAnimScale, num_control_pts);
      read_and_advance_raw(&buf, control_pts, num_control_pts * sizeof(XMFLOAT3));
    }
  }

  ANIMATION_MANAGER.on_loaded();

  return true;
}

bool KumiLoader::load_animation3(const char *buf, Scene *scene) {

  BlockHeader *header = (BlockHeader *)buf;
  buf += sizeof(BlockHeader);
  const char *buf_end = buf + header->size;

  int anim_bits = _header.animation_bits;

  while (buf < buf_end) {

    string node_name = read_and_advance<const char *>(&buf);

    if (int num_pos_keys = read_and_advance<int>(&buf)) {
      auto *keyframes = ANIMATION_MANAGER.alloc_anim(node_name, AnimationManager::kAnimPos, num_pos_keys);
      read_and_advance_raw(&buf, keyframes, num_pos_keys * sizeof(KeyFrameVec3));
    }

    if (int num_rot_keys = read_and_advance<int>(&buf)) {
      auto *keyframes = ANIMATION_MANAGER.alloc_anim(node_name, AnimationManager::kAnimRot, num_rot_keys);
      read_and_advance_raw(&buf, keyframes, num_rot_keys * sizeof(KeyFrameVec4));
    }

    if (int num_scale_keys = read_and_advance<int>(&buf)) {
      auto *keyframes = ANIMATION_MANAGER.alloc_anim(node_name, AnimationManager::kAnimScale, num_scale_keys);
      read_and_advance_raw(&buf, keyframes, num_scale_keys * sizeof(KeyFrameVec3));
    }
  }

  ANIMATION_MANAGER.on_loaded();

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
    string material_name = read_and_advance<const char *>(&buf);
    string technique = read_and_advance<const char *>(&buf);
    _technique_for_material[material_name] = technique;
    Material *material = new Material(material_name);
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

        resource = RESOURCE_MANAGER.load_texture(filename.c_str(), name, true, &info);
        material->add_flag(GRAPHICS.get_shader_flag("DIFFUSE_TEXTURE"));
        if (!resource.is_valid())
          LOG_WARNING_LN("Unable to load texture: %s", filename.c_str());
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

bool KumiLoader::load(const char *filename, const char *material_override, Scene **scene) {
  _filename = filename;
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
            //auto &name = property->get("name")->to_string();
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

  vector<char> buffer;
  B_ERR_BOOL(RESOURCE_MANAGER.load_file(filename, &buffer));
  _header = *(MainHeader *)buffer.data();
  if (_header.version != FILE_VERSION) {
    LOG_ERROR_LN("Incompatible kumi file version: want: %d, got: %d", FILE_VERSION, _header.version);
    return false;
  }

  // We save offsets to binary data (like strings) in the .kumi file, as well as the location of the offsets,
  // so now we convert those offsets to real pointers into the buffer data.
  const void *binaryData = &buffer[_header.binary_ofs];
  apply_fixup(binaryData, buffer.data(), binaryData);

  Scene *s = *scene = new Scene;

  B_ERR_BOOL(load_globals(buffer.data() + _header.global_ofs, s));
  B_ERR_BOOL(load_materials(buffer.data() + _header.material_ofs, s));
  B_ERR_BOOL(load_meshes(buffer.data() + _header.mesh_ofs, s));
  B_ERR_BOOL(load_cameras(buffer.data() + _header.camera_ofs, s));
  B_ERR_BOOL(load_lights(buffer.data() + _header.light_ofs, s));
  B_ERR_BOOL(load_animation3(buffer.data() + _header.animation_ofs, s));

  B_ERR_BOOL(s->on_loaded());

  return true;
}
