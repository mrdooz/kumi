#pragma once
#include "graphics_object_handle.hpp"
#include "property_manager.hpp"

static const int MAX_SAMPLERS = 8;
static const int MAX_TEXTURES = 8;

struct MeshRenderData {
	MeshRenderData() : topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST), cbuffer_staged(nullptr), cbuffer_len(0) {}
	GraphicsObjectHandle vb, ib;
	DXGI_FORMAT index_format;
	int index_count;
	int vertex_size;
	int vertex_count;
	D3D_PRIMITIVE_TOPOLOGY topology;
	GraphicsObjectHandle technique;
	uint16 material_id;
	PropertyManager::Token mesh_id;
  void *cbuffer_staged;
  int cbuffer_len;
};

struct TechniqueRenderData {
	GraphicsObjectHandle technique;
	uint16 material_id;
  GraphicsObjectHandle textures[MAX_TEXTURES];
  GraphicsObjectHandle render_targets[MAX_TEXTURES];
};

struct BufferRenderData {
  BufferRenderData() : index_count(0), start_index(0), start_vertex(0), base_vertex(0), vertex_size(0), vertex_count(0) {}
  GraphicsObjectHandle vs, ps;
  GraphicsObjectHandle vb, ib;
  GraphicsObjectHandle layout;
  GraphicsObjectHandle rs, dss, bs, ss;
  // samplers and textures are loaded into consecutive registers
  GraphicsObjectHandle samplers[MAX_SAMPLERS];
  GraphicsObjectHandle textures[MAX_TEXTURES];
  DXGI_FORMAT index_format;
  int index_count;
  int start_index;
  int start_vertex;
  int base_vertex;
  int vertex_size;
  int vertex_count;
  D3D_PRIMITIVE_TOPOLOGY topology;
};

struct TextRenderData {
  GraphicsObjectHandle font;
  const WCHAR *str;  // lives on the allocated stack. no need to free
  float x, y;
  float font_size;
  uint32 color; // AABBGGRR
  uint32 flags;
};

// from http://realtimecollisiondetection.net/blog/?p=86
struct RenderKey {
	enum Cmd {
		kDelete,	// submitted when the object is deleted to avoid using stale data
		kSetRenderTarget,
		kRenderMesh,
		kRenderTechnique,
    kRenderBuffers,
    kRenderText,
    kSetScissorRect,
		kNumCommands
	};
	static_assert(kNumCommands < (1 << 16), "Too many commands");

	RenderKey() : data(0) {}

	union {
		// Written for little endianess
		struct {
			union {
				struct {
					uint32 padding : 6;
					uint32 material : 16;
					uint32 shader : 10;
				};
				uint32 handle : 32;  // a GraphicsObjectHandle
			};
			uint16 cmd;
			union {
				uint16 seq_nr;
				uint16 depth;
			};
		};
		uint64 data;
	};
};

static_assert(sizeof(RenderKey) <= sizeof(uint64_t), "RenderKey too large");
