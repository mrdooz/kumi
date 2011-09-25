#pragma once

class GraphicsObjectHandle {
	friend class Graphics;
	enum { 
		cTypeBits = 8,
		cGenerationBits = 8,
		cIdBits = 12 
	};
	enum Type {
		kInvalid = -1,
		kContext,
		kVertexBuffer,
		kIndexBuffer,
		kConstantBuffer,
		kTexture,
		kRenderTarget,
		kShader,
		kInputLayout,
		kBlendState,
		kRasterizerState,
		kSamplerState,
		kDepthStencilState,
		kTechnique,
		kVertexShader,
		kPixelShader,
		kShaderResourceView,
	};	
	GraphicsObjectHandle(uint32 type, uint32 generation, uint32 id) : _type(type), _generation(generation), _id(id) {}
	union {
		struct {
			uint32 _type : 8;
			uint32 _generation : 8;
			uint32 _id : 12;
		};
		uint32 _data;
	};
public:
	GraphicsObjectHandle() : _data(kInvalid) {}
	GraphicsObjectHandle(uint32 data) : _data(data) {}
	bool is_valid() const { return _data != kInvalid; }
	operator int() const { return _data; }
	uint32 id() const { return _id; }
};

static_assert(sizeof(GraphicsObjectHandle) <= sizeof(uint32_t), "GraphicsObjectHandle too large");
