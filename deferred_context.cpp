#include "stdafx.h"
#include "graphics.hpp"
#include "logger.hpp"
#include "mesh.hpp"
#include "deferred_context.hpp"
#include "material_manager.hpp"
#include "scene.hpp"

using namespace std;

DeferredContext::DeferredContext() 
  : _ctx(nullptr)
  , _prev_topology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
  , _is_immediate_context(false)
{
    _default_stencil_ref = GRAPHICS.default_stencil_ref();
    memcpy(_default_blend_factors, GRAPHICS.default_blend_factors(), sizeof(_default_blend_factors));
    _default_sample_mask = GRAPHICS.default_sample_mask();
}

DeferredContext::~DeferredContext() {
}


void DeferredContext::render_technique(GraphicsObjectHandle technique_handle, 
                                       const TextureArray &resources,
                                       const InstanceData &instance_data) {
  Technique *technique = GRAPHICS._techniques.get(technique_handle);

  Shader *vs = technique->vertex_shader(0);
  Shader *ps = technique->pixel_shader(0);
  set_vs(vs->handle());
  set_ps(ps->handle());

  set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  set_layout(vs->input_layout());
  set_vb(technique->vb(), technique->vertex_size());
  set_ib(technique->ib(), technique->index_format());

  set_rs(technique->rasterizer_state());
  set_dss(technique->depth_stencil_state(), _default_stencil_ref);
  set_bs(technique->blend_state(), _default_blend_factors, _default_sample_mask);

  // set samplers
  auto &samplers = ps->samplers();
  if (!samplers.empty()) {
    set_samplers(ps->samplers());
  }

  // set resource views
  auto &rv = ps->resource_views();
  TextureArray all_views(resources);
  fill_system_resource_views(rv, &all_views);
  bool has_resources = false;
  for (size_t i = 0; i < all_views.size(); ++i) {
    if (all_views[i].is_valid()) {
      has_resources = true;
      set_shader_resources(all_views);
      break;
    }
  }

  // set cbuffers
  auto &vs_s_cbuffer = vs->system_cbuffer();
  technique->fill_cbuffer(&vs->system_cbuffer());

  auto &ps_s_cbuffer = ps->system_cbuffer();
  technique->fill_cbuffer(&ps_s_cbuffer);

  // Check if we're drawing instanced
  if (instance_data.num_instances) {

    set_cbuffer(vs_s_cbuffer, ps_s_cbuffer);

    for (int ii = 0; ii < instance_data.num_instances; ++ii) {

      auto &ps_i_cbuffer = ps->instance_cbuffer();

      for (size_t j = 0; j < ps_i_cbuffer.vars.size(); ++j) {
        auto &var = ps_i_cbuffer.vars[j];

        for (size_t k = 0; k < instance_data.vars.size(); ++k) {
          auto &cur = instance_data.vars[k];
          if (cur.id == var.id) {
            const void *data = &instance_data.payload[cur.ofs + ii * instance_data.block_size];
            memcpy(&ps_i_cbuffer.staging[var.ofs], data, cur.len);
            break;
          }
        }
      }

      set_cbuffer(vs->instance_cbuffer(), ps->instance_cbuffer());
      draw_indexed(technique->index_count(), 0, 0);
    }

  } else {

    set_cbuffers(vs->cbuffers(), ps->cbuffers());
    draw_indexed(technique->index_count(), 0, 0);
  }

  if (has_resources)
    unset_shader_resource(0, MAX_TEXTURES);
}

void DeferredContext::fill_system_resource_views(const ResourceViewArray &views, TextureArray *out) const {
  for (size_t i = 0; i < views.size(); ++i) {
    if (views[i].used && views[i].source == PropertySource::kSystem) {
      (*out)[i] = PROPERTY_MANAGER.get_property<GraphicsObjectHandle>(views[i].class_id);
    }
  }
}

void DeferredContext::generate_mips(GraphicsObjectHandle h) {
  auto rt = GRAPHICS._render_targets.get(h);
  _ctx->GenerateMips(rt->srv.resource);
}

void DeferredContext::set_render_target(GraphicsObjectHandle render_target, bool clear_target) {
  set_render_targets(&render_target, &clear_target, 1);
}

void DeferredContext::set_render_targets(GraphicsObjectHandle *render_targets, bool *clear_targets, int num_render_targets) {

  ID3D11RenderTargetView *rts[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
  ID3D11DepthStencilView *dsv = nullptr;
  D3D11_TEXTURE2D_DESC texture_desc;
  float color[4] = {0,0,0,0};
  // Collect the valid render targets, set the first available depth buffer
  // and clear targets if specified
  for (int i = 0; i < num_render_targets; ++i) {
    GraphicsObjectHandle h = render_targets[i];
    KASSERT(h.is_valid());
    auto rt = GRAPHICS._render_targets.get(h);
    texture_desc = rt->texture.desc;
    if (!dsv && rt->dsv.resource) {
      dsv = rt->dsv.resource;
    }
    rts[i] = rt->rtv.resource;
    // clear render target (and depth stenci)
    if (clear_targets[i]) {
      _ctx->ClearRenderTargetView(rts[i], color);
      if (rt->dsv.resource) {
        _ctx->ClearDepthStencilView(rt->dsv.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
      }
    }
  }
  CD3D11_VIEWPORT viewport(0.0f, 0.0f, (float)texture_desc.Width, (float)texture_desc.Height);
  _ctx->RSSetViewports(1, &viewport);
  _ctx->OMSetRenderTargets(num_render_targets, rts, dsv);
}

void DeferredContext::set_default_render_target() {
  auto rt = GRAPHICS._render_targets.get(GRAPHICS._default_render_target);
  _ctx->OMSetRenderTargets(1, &rt->rtv.resource.p, rt->dsv.resource);
  _ctx->RSSetViewports(1, &GRAPHICS._viewport);

}

void DeferredContext::set_vs(GraphicsObjectHandle vs) {
  if (prev_vs != vs) {
    _ctx->VSSetShader(GRAPHICS._vertex_shaders.get(vs), NULL, 0);
    prev_vs = vs;
  }
}

void DeferredContext::set_ps(GraphicsObjectHandle ps) {
  if (prev_ps != ps) {
    _ctx->PSSetShader(GRAPHICS._pixel_shaders.get(ps), NULL, 0);
    prev_ps = ps;
  }
}

void DeferredContext::set_layout(GraphicsObjectHandle layout) {
  if (prev_layout != layout) {
    _ctx->IASetInputLayout(GRAPHICS._input_layouts.get(layout));
    prev_layout = layout;
  }
}

void DeferredContext::set_vb(ID3D11Buffer *buf, uint32_t stride)
{
  UINT ofs[] = { 0 };
  ID3D11Buffer* bufs[] = { buf };
  uint32_t strides[] = { stride };
  _ctx->IASetVertexBuffers(0, 1, bufs, strides, ofs);
}

void DeferredContext::set_vb(GraphicsObjectHandle vb, int vertex_size) {
  if (prev_vb != vb) {
    set_vb(GRAPHICS._vertex_buffers.get(vb), vertex_size);
    prev_vb = vb;
  }
}

void DeferredContext::set_ib(GraphicsObjectHandle ib, DXGI_FORMAT format) {
  if (prev_ib != ib) {
    _ctx->IASetIndexBuffer(GRAPHICS._index_buffers.get(ib), format, 0);
    prev_ib = ib;
  }
}

void DeferredContext::set_topology(D3D11_PRIMITIVE_TOPOLOGY top) {
  if (_prev_topology != top) {
    _ctx->IASetPrimitiveTopology(top);
    _prev_topology = top;
  }
}

void DeferredContext::set_rs(GraphicsObjectHandle rs) {
  if (prev_rs != rs) {
    _ctx->RSSetState(GRAPHICS._rasterizer_states.get(rs));
    prev_rs = rs;
  }
}

void DeferredContext::set_dss(GraphicsObjectHandle dss, UINT stencil_ref) {
  if (prev_dss != dss) {
    _ctx->OMSetDepthStencilState(GRAPHICS._depth_stencil_states.get(dss), stencil_ref);
    prev_dss = dss;
  }
}

void DeferredContext::set_bs(GraphicsObjectHandle bs, const float *blend_factors, UINT sample_mask) {
  if (prev_bs != bs) {
    _ctx->OMSetBlendState(GRAPHICS._blend_states.get(bs), blend_factors, sample_mask);
    prev_bs = bs;
  }
}

void DeferredContext::set_samplers(const SamplerArray &samplers) {
  int size = samplers.size() * sizeof(GraphicsObjectHandle);
  if (memcmp(samplers.data(), prev_samplers, size) != 0) {
    int first_sampler = MAX_SAMPLERS, num_samplers = 0;
    ID3D11SamplerState *d3dsamplers[MAX_SAMPLERS];
    for (int i = 0; i < MAX_SAMPLERS; ++i) {
      if (samplers[i].is_valid()) {
        d3dsamplers[i] = GRAPHICS._sampler_states.get(samplers[i]);
        first_sampler = min(first_sampler, i);
        num_samplers++;
      }
    }

    if (num_samplers) {
      _ctx->PSSetSamplers(first_sampler, num_samplers, &d3dsamplers[first_sampler]);
      memcpy(prev_samplers, samplers.data(), size);
    }
  }
}

void DeferredContext::set_shader_resources(const TextureArray &resources) {
  int size = resources.size() * sizeof(GraphicsObjectHandle);
  // force setting the views because we always unset them..
  bool force = true;
  if (force || memcmp(resources.data(), prev_resources, size) != 0) {
    ID3D11ShaderResourceView *d3dresources[MAX_TEXTURES];
    int first_resource = MAX_TEXTURES, num_resources = 0;
    for (int i = 0; i < MAX_TEXTURES; ++i) {
      if (resources[i].is_valid()) {
        GraphicsObjectHandle h = resources[i];
        if (h.type() == GraphicsObjectHandle::kTexture) {
          auto *data = GRAPHICS._textures.get(h);
          d3dresources[i] = data->view.resource;
        } else if (h.type() == GraphicsObjectHandle::kResource) {
          auto *data = GRAPHICS._resources.get(h);
          d3dresources[i] = data->view.resource;
        } else if (h.type() == GraphicsObjectHandle::kRenderTarget) {
          auto *data = GRAPHICS._render_targets.get(h);
          d3dresources[i] = data->srv.resource;
        } else {
          LOG_ERROR_LN("Trying to set invalid resource type!");
        }
        num_resources++;
        first_resource = min(first_resource, i);
      }
    }

    if (num_resources) {
      int ofs = first_resource;
      int num_resources_set = 0;
      while (ofs < MAX_TEXTURES && num_resources_set < num_resources) {
        // handle non sequential resources
        int cur = 0;
        int tmp = ofs;
        while (resources[ofs].is_valid()) {
          ofs++;
          cur++;
          if (ofs == MAX_TEXTURES)
            break;
        }
        _ctx->PSSetShaderResources(tmp, cur, &d3dresources[tmp]);
        num_resources_set += cur;
        while (ofs < MAX_TEXTURES && !resources[ofs].is_valid())
          ofs++;
      }
      memcpy(prev_resources, resources.data(), size);
    }
  }
}

void DeferredContext::unset_shader_resource(int first_view, int num_views) {
  if (!num_views)
    return;
  static ID3D11ShaderResourceView *null_views[MAX_SAMPLERS] = {0, 0, 0, 0, 0, 0, 0, 0};
  _ctx->PSSetShaderResources(first_view, num_views, null_views);
}

void DeferredContext::set_cbuffer(const CBuffer &vs, const CBuffer &ps) {

  if (vs.staging.size() > 0) {
    ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(vs.handle);
    D3D11_MAPPED_SUBRESOURCE sub;
    _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, vs.staging.data(), vs.staging.size());
    _ctx->Unmap(buffer, 0);

    _ctx->VSSetConstantBuffers(vs.slot, 1, &buffer);
  }

  if (ps.staging.size() > 0) {
    ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(ps.handle);
    D3D11_MAPPED_SUBRESOURCE sub;
    _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, ps.staging.data(), ps.staging.size());
    _ctx->Unmap(buffer, 0);

    _ctx->PSSetConstantBuffers(ps.slot, 1, &buffer);
  }
}

void DeferredContext::set_cbuffers(const std::vector<CBuffer *> &vs, const std::vector<CBuffer *> &ps) {

  ID3D11Buffer **vs_cb = (ID3D11Buffer **)_alloca(vs.size() * sizeof(ID3D11Buffer *));
  ID3D11Buffer **ps_cb = (ID3D11Buffer **)_alloca(ps.size() * sizeof(ID3D11Buffer *));

  // Copy the vs cbuffers
  for (size_t i = 0; i < vs.size(); ++i) {
    auto &cur = vs[i];
    ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(cur->handle);
    D3D11_MAPPED_SUBRESOURCE sub;
    _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, cur->staging.data(), cur->staging.size());
    _ctx->Unmap(buffer, 0);
    vs_cb[i] = buffer;
  }

  // Copy the ps cbuffers
  for (size_t i = 0; i < ps.size(); ++i) {
    auto &cur = ps[i];
    ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(cur->handle);
    D3D11_MAPPED_SUBRESOURCE sub;
    _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, cur->staging.data(), cur->staging.size());
    _ctx->Unmap(buffer, 0);
    ps_cb[i] = buffer;
  }

  if (!vs.empty())
    _ctx->VSSetConstantBuffers(0, vs.size(), vs_cb);

  if (!ps.empty())
      _ctx->PSSetConstantBuffers(0, ps.size(), ps_cb);
}

void DeferredContext::set_cbuffer(GraphicsObjectHandle cb, int slot, ShaderType::Enum type, const void *data, int dataLen) {

  ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(cb);
  D3D11_MAPPED_SUBRESOURCE sub;
  _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
  memcpy(sub.pData, data, dataLen);
  _ctx->Unmap(buffer, 0);

  if (type == ShaderType::kVertexShader)
    _ctx->VSSetConstantBuffers(slot, 1, &buffer);
  else if (type == ShaderType::kPixelShader)
    _ctx->PSSetConstantBuffers(slot, 1, &buffer);
  else if (type == ShaderType::kComputeShader)
    _ctx->CSSetConstantBuffers(slot, 1, &buffer);
}

void DeferredContext::draw_indexed(int count, int start_index, int base_vertex) {
  _ctx->DrawIndexed(count, start_index, base_vertex);
}

void DeferredContext::begin_frame() {

  prev_vs = prev_ps = prev_layout = GraphicsObjectHandle();
  prev_rs = prev_bs = prev_dss = GraphicsObjectHandle();
  prev_ib = prev_vb = GraphicsObjectHandle();

  for (size_t i = 0; i < MAX_SAMPLERS; ++i)
    prev_samplers[i] = GraphicsObjectHandle();

  for (size_t i = 0; i < MAX_TEXTURES; ++i)
    prev_resources[i] = GraphicsObjectHandle();

  _prev_topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
/*
  for (size_t i = 0; i < CBUFFER_CACHE; ++i) {
    _vs_cbuffer_cache[i].clear();
    _ps_cbuffer_cache[i].clear();
  }
*/
}

void DeferredContext::end_frame() {
  if (!_is_immediate_context) {
    ID3D11CommandList *cmd_list;
    _ctx->FinishCommandList(FALSE, &cmd_list);
    GRAPHICS.add_command_list(cmd_list);
  }
}
