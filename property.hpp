#pragma once
#include "graphics_object_handle.hpp"

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

  std::string to_string(Enum e);
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
  int ofs;
  int len;
  PropertyId id;
#ifdef _DEBUG
  std::string name;
#endif
};

struct CBuffer {
  GraphicsObjectHandle handle;
  std::vector<CBufferVariable> mesh_vars;
  std::vector<CBufferVariable> material_vars;
  std::vector<CBufferVariable> system_vars;
  std::vector<CBufferVariable> instance_vars;
  std::vector<char> staging;
};
