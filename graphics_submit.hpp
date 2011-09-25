#pragma once
#include "graphics_object_handle.hpp"
#include "property_manager.hpp"

struct MeshRenderData {
	MeshRenderData() : topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}
	GraphicsObjectHandle vb, ib;
	DXGI_FORMAT index_format;
	int index_count;
	int vertex_size;
	int vertex_count;
	D3D_PRIMITIVE_TOPOLOGY topology;
	GraphicsObjectHandle technique;
	uint16 material_id;
	PropertyManager::Id mesh_id;
};

struct TechniqueRenderData {
	GraphicsObjectHandle technique;
	uint16 material_id;
};

// from http://realtimecollisiondetection.net/blog/?p=86
struct RenderKey {
	enum Cmd {
		kDelete,	// submitted when the object is deleted to avoid using stale data
		kSetRenderTarget,
		kRenderMesh,
		kRenderTechnique,
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
