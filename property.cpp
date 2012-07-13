#include "stdafx.h"
#include "property.hpp"

namespace PropertySource {
  const char *to_string(Enum e) {
    switch (e) {
    case kUnknown: return "Unknown";
    case kMaterial: return "Material";
    case kSystem: return "System";
    case kInstance: return "Instance";
    case kUser: return "User";
    case kMesh: return "Mesh";
    }
    return "INVALID";
  }
  std::string qualify_name(const std::string &name, PropertySource::Enum source) {
    return std::string(PropertySource::to_string(source)) + "::" + name;
  }
}

namespace PropertyType {
  int len(Enum type) {
    int num_elems = max(1, HIWORD(type));

    switch (LOWORD(type)) {
    case kFloat: return num_elems * sizeof(float);
    case kFloat2: return num_elems * sizeof(XMFLOAT2);
    case kFloat3: return num_elems * sizeof(XMFLOAT3);
    case kFloat4: return num_elems * sizeof(XMFLOAT4);
    case kFloat4x4: return num_elems * sizeof(XMFLOAT4X4);
    case kInt: return num_elems * sizeof(int);
    default:
      return 0;
    }
  }

}