#pragma once

namespace PropertyType {
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

  inline int len(Enum type) {
    switch (type) {
    case kFloat: return sizeof(float);
    case kFloat2: return sizeof(XMFLOAT2);
    case kFloat3: return sizeof(XMFLOAT3);
    case kFloat4: return sizeof(XMFLOAT4);
    case kFloat4x4: return sizeof(XMFLOAT4X4);
    case kInt: return sizeof(int);
    default:
      return -1;
    }
  }
}
