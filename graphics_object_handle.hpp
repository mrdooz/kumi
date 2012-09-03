#pragma once

class GraphicsObjectHandle {
public:
  enum Type {
    kInvalid = -1,    // NB: You have to compare _raw against kInvalid to test
    kContext,
    kVertexBuffer,
    kIndexBuffer,
    kConstantBuffer,
    kTexture,
    kResource,
    kRenderTarget,
    kShader,
    kInputLayout,
    kBlendState,
    kRasterizerState,
    kSamplerState,
    kDepthStencilState,
    kTechnique,
    kVertexShader,
    kGeometryShader,
    kPixelShader,
    kComputeShader,
    kFontFamily,
    kMaterial,
    kStructuredBuffer,
    cNumTypes
  };	
private:
  friend class Graphics;
  friend class MaterialManager;
  enum { 
    cTypeBits = 8,
    cIdBits = 12,
    cDataBits = 32 - (cTypeBits + cIdBits),
  };
  static_assert(1 << GraphicsObjectHandle::cTypeBits > GraphicsObjectHandle::cNumTypes, "Not enough type bits");

  GraphicsObjectHandle(uint32 type, uint32 id) : _type(type), _id(id), _data(0) {}
  GraphicsObjectHandle(uint32 type, uint32 id, uint32 data) : _type(type), _id(id), _data(data) 
  {
    KASSERT(data < (1 << cDataBits));
  }

  union {
    struct {
      uint32 _type : cTypeBits;
      uint32 _id : cIdBits;
      uint32 _data : cDataBits;
    };
    uint32 _raw;
  };
public:
  GraphicsObjectHandle() : _raw(kInvalid) {}
  bool is_valid() const { return _raw != kInvalid; }
  operator int() const { return _raw; }
  uint32 id() const { return _id; }
  uint32 data() const { return _data; }
  Type type() const { return (Type)_type; }
};

static_assert(sizeof(GraphicsObjectHandle) <= sizeof(uint32_t), "GraphicsObjectHandle too large");

