#pragma once
#include "graphics_object_handle.hpp"
#include "shader.hpp"


class DeferredContext {
  friend class Graphics;
public:

  struct InstanceVar {
    PropertyId id;
    int len;
    int ofs;
  };

  struct InstanceData {
    int num_instances;
    int block_size;
    const char *payload;
    std::vector<InstanceVar> vars;
  };

  void set_default_render_target();
  void set_render_targets(GraphicsObjectHandle *render_targets, bool *clear_targets, int num_render_targets);
  void generate_mips(GraphicsObjectHandle h);

  void render_mesh(Mesh *mesh, GraphicsObjectHandle technique_handle);
  void render_technique(GraphicsObjectHandle technique_handle,
    const TextureArray &resources,
    const InstanceData &instance_data);

private:
  DeferredContext();
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
  void set_cbuffer(const std::vector<CBuffer> &vs, const std::vector<CBuffer> &ps);
  void draw_indexed(int count, int start_index, int base_vertex);

  void fill_system_resource_views(const ResourceViewArray &views, TextureArray *out) const;

  GraphicsObjectHandle prev_vs, prev_ps, prev_layout;
  GraphicsObjectHandle prev_rs, prev_bs, prev_dss;
  GraphicsObjectHandle prev_ib, prev_vb;
  GraphicsObjectHandle prev_samplers[MAX_SAMPLERS];
  GraphicsObjectHandle prev_resources[MAX_TEXTURES];
  D3D11_PRIMITIVE_TOPOLOGY prev_topology;
};
