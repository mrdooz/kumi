#pragma once
#include "graphics_object_handle.hpp"
#include "shader.hpp"

struct Scene;

class DeferredContext {
  friend class Graphics;
public:

  struct InstanceVar {
    InstanceVar(PropertyId id, int len) : id(id), len(len), ofs(0) {}
    PropertyId id;
    int len;
    int ofs;
  };

  struct InstanceData {
    InstanceData() : num_instances(0), block_size(0) {}

    void add_variable(PropertyId id, int len) {
      vars.push_back(InstanceVar(id, len));
    }

    void alloc(int count) {
      num_instances = count;
      block_size = 0;
      int ofs = 0;
      for (size_t i = 0; i < vars.size(); ++i) {
        vars[i].ofs = ofs;
        ofs += vars[i].len;
        block_size += vars[i].len;
      }
      payload.resize(block_size * num_instances);
    }

    char *data(int idx, int instance) {
      return payload.data() + vars[idx].ofs + instance * block_size;
    }

    int num_instances;
    int block_size; // size of a single batch of instance variables
    std::vector<char> payload;
    std::vector<InstanceVar> vars;
  };

  void set_default_render_target();
  void set_render_targets(GraphicsObjectHandle *render_targets, bool *clear_targets, int num_render_targets);
  void generate_mips(GraphicsObjectHandle h);

  void render_scene(Scene *scene, GraphicsObjectHandle technique_handle);
  void render_mesh(Mesh *mesh, GraphicsObjectHandle technique_handle);
  void render_technique(GraphicsObjectHandle technique_handle,
    const TextureArray &resources,
    const InstanceData &instance_data);

  void begin_frame();
  void end_frame();

private:
  DeferredContext();
  ~DeferredContext();
  ID3D11DeviceContext *_ctx;

  void set_vb(ID3D11Buffer *buf, uint32_t stride);
  void set_vs(GraphicsObjectHandle vs);
  void set_ps(GraphicsObjectHandle ps);
  void set_layout(GraphicsObjectHandle layout);
  void set_vb(GraphicsObjectHandle vb, int vertex_size);
  void set_ib(GraphicsObjectHandle ib, DXGI_FORMAT format);
  void set_topology(D3D11_PRIMITIVE_TOPOLOGY top);
  void set_rs(GraphicsObjectHandle rs);
  void set_dss(GraphicsObjectHandle dss, UINT stencil_ref);
  void set_bs(GraphicsObjectHandle bs, const float *blend_factors, UINT sample_mask);
  void set_samplers(const SamplerArray &samplers);
  void set_shader_resources(const TextureArray &resources);
  void unset_shader_resource(int first_view, int num_views);
  void set_cbuffers(const std::vector<CBuffer *> &vs, const std::vector<CBuffer *> &ps);
  void set_cbuffer(const CBuffer &vs, const CBuffer &ps);
  void draw_indexed(int count, int start_index, int base_vertex);

  void fill_system_resource_views(const ResourceViewArray &views, TextureArray *out) const;

  GraphicsObjectHandle prev_vs, prev_ps, prev_layout;
  GraphicsObjectHandle prev_rs, prev_bs, prev_dss;
  GraphicsObjectHandle prev_ib, prev_vb;
  GraphicsObjectHandle prev_samplers[MAX_SAMPLERS];
  GraphicsObjectHandle prev_resources[MAX_TEXTURES];
  D3D11_PRIMITIVE_TOPOLOGY _prev_topology;

  uint32 _default_stencil_ref;
  float _default_blend_factors[4];
  uint32 _default_sample_mask;

  bool _is_immediate_context;
};
