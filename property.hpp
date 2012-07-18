#pragma once
#include "graphics_object_handle.hpp"

static const int MAX_SAMPLERS = 8;
static const int MAX_TEXTURES = 8;

typedef std::array<GraphicsObjectHandle, MAX_TEXTURES> TextureArray;
typedef std::array<GraphicsObjectHandle, MAX_SAMPLERS> SamplerArray;

typedef uint32 PropertyId;

namespace PropertySource {
  enum Enum {
    kUnknown,
    kMaterial,
    kSystem,
    kInstance,
    kUser,
    kMesh,
  };

  const char *to_string(Enum e);
  std::string qualify_name(const std::string &name, PropertySource::Enum source);
}

namespace PropertyType {
  // LOWORD is type, HIWORD is length of array
  enum Enum {
    kUnknown,
    kFloat,
    kFloat2,
    kFloat3,
    kFloat4,
    kColor,
    kFloat4x4,
    kTexture2d,
    kSampler,
    kInt,
  };

  int len(Enum type);
}

struct CBufferVariable {
  CBufferVariable(int ofs, int len, PropertyId id) : ofs(ofs), len(len), id(id) {}
  int ofs;
  int len;
  PropertyId id;
#ifdef _DEBUG
  std::string name;
#endif
};

struct CBuffer {
  CBuffer() : slot(~0) {}
  void init() {
    auto &fn = [&](const CBufferVariable &a, const CBufferVariable &b) { return a.id < b.id; };
    sort(begin(vars), end(vars), fn);
  }
  GraphicsObjectHandle handle;
  int slot;
  std::vector<CBufferVariable> vars;
  std::vector<char> staging;
};

template<typename T>
struct ResourceId {
  ResourceId(T id, int bind_point, const std::string &name)
    : id(id)
    , bind_point(bind_point)
#ifdef _DEBUG
    , name(name)
#endif
  {}
  T id;
  int bind_point;
#ifdef _DEBUG
  std::string name;
#endif
};

struct ResourceViews {
  ResourceViews() { views.resize(MAX_TEXTURES); }
  std::vector<ResourceId<PropertyId>> material_views; // ids are class-ids
  std::vector<ResourceId<GraphicsObjectHandle>> system_views;
  std::vector<int> user_views;
  std::vector<GraphicsObjectHandle> views;
};
