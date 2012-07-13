#pragma once
#include "graphics_object_handle.hpp"
#include "property_manager.hpp"

class Mesh;
class SubMesh;
class Material;

#pragma pack(push, 1)

struct MeshGeometry {
  MeshGeometry() : topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}
  GraphicsObjectHandle vb, ib;
  DXGI_FORMAT index_format;
  int index_count;
  int vertex_size;
  int vertex_count;
  D3D_PRIMITIVE_TOPOLOGY topology;
};

struct MeshRenderData {
  const MeshGeometry *geometry;
  GraphicsObjectHandle technique;
  GraphicsObjectHandle material;
  const Mesh *mesh;
#if _DEBUG
  const SubMesh *submesh;
#endif
};

struct TechniqueRenderData {
  TechniqueRenderData() : user_textures(0), num_instances(0), num_instance_variables(0) {}
  int user_textures;
  GraphicsObjectHandle textures[MAX_TEXTURES];
  int num_instances;
  int num_instance_variables;
#pragma warning(suppress: 4200)
  char payload[];
  // followed by instance variables of the form
  // PropertyId id;
  // T t[num_instance_variables]; // value per instance
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

struct RenderTargetData {
  RenderTargetData() {
    for (int i = 0; i < 8; ++i) {
      clear_target[i] = false;
    }
  }
  GraphicsObjectHandle render_targets[8]; // if there's a depth buffer, it's attached to rt0
  bool clear_target[8];
};

// from http://realtimecollisiondetection.net/blog/?p=86
struct RenderKey {
  enum Cmd {
    kSetRenderTarget,
    kRenderMesh,
    kRenderTechnique, // this should probably be changed to "render fullscreen"
    kRenderBuffers,
    kRenderText,
    kSetScissorRect,
    kReleaseRenderTarget,
    kGenerateMips,
    kNumCommands
  };
  static_assert(kNumCommands < (1 << 16), "Too many commands");

  RenderKey() : data(0) {}
  RenderKey(Cmd cmd) : data(0) {
    this->cmd = cmd;
  }
  RenderKey(Cmd cmd, GraphicsObjectHandle h) : data(0) {
    this->cmd = cmd;
    this->handle = h;
  }

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

#pragma pack(pop)
static_assert(sizeof(RenderKey) <= sizeof(uint64_t), "RenderKey too large");
