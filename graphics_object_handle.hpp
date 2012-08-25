#pragma once

class GraphicsObjectHandle {
public:
  enum Type {
    kInvalid = -1,
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
  };	
private:
  friend class Graphics;
  friend class MaterialManager;
  enum { 
    cTypeBits = 8,
    cGenerationBits = 8,
    cIdBits = 16 
  };
  GraphicsObjectHandle(uint32 type, uint32 generation, uint32 id) : _type(type), _generation(generation), _id(id) {}
  union {
    struct {
      uint32 _type : 8;
      uint32 _generation : 8;
      uint32 _id : 16;
    };
    uint32 _data;
  };
public:
  GraphicsObjectHandle() : _data(kInvalid) {}
  GraphicsObjectHandle(uint32 data) : _data(data) {}
  bool is_valid() const { return _data != kInvalid; }
  operator int() const { return _data; }
  uint32 id() const { return _id; }
  Type type() const { return (Type)_type; }
};

static_assert(sizeof(GraphicsObjectHandle) <= sizeof(uint32_t), "GraphicsObjectHandle too large");
