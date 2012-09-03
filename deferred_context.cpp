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
  , _is_immediate_context(false)
{
    _default_stencil_ref = GRAPHICS.default_stencil_ref();
    memcpy(_default_blend_factors, GRAPHICS.default_blend_factors(), sizeof(_default_blend_factors));
    _default_sample_mask = GRAPHICS.default_sample_mask();
}

DeferredContext::~DeferredContext() {
}


void DeferredContext::render_technique(GraphicsObjectHandle technique_handle,
                                       const std::function<void(CBuffer *)> &fnSystemCbuffers,
                                       const TextureArray &resources,
                                       const InstanceData &instance_data) {
  Technique *technique = GRAPHICS._techniques.get(technique_handle);

  Shader *vs = technique->vertex_shader(0);
  Shader *ps = technique->pixel_shader(0);
  Shader *gs = technique->geometry_shader(0);
  set_vs(vs->handle());
  set_ps(ps->handle());
  set_gs(gs ? gs->handle() : GraphicsObjectHandle());

  set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  set_layout(vs->input_layout());
  set_vb(technique->vb());
  set_ib(technique->ib());

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
      set_shader_resources(all_views, ShaderType::kPixelShader);
      break;
    }
  }

  // set cbuffers
  auto &vs_s_cbuffer = vs->system_cbuffer();
  fnSystemCbuffers(&vs_s_cbuffer);

  auto &ps_s_cbuffer = ps->system_cbuffer();
  fnSystemCbuffers(&ps_s_cbuffer);

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
    unset_shader_resource(0, MAX_TEXTURES, ShaderType::kPixelShader);
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

void DeferredContext::set_default_render_target(bool clear) {
  auto rt = GRAPHICS._render_targets.get(GRAPHICS._default_render_target);
  _ctx->OMSetRenderTargets(1, &rt->rtv.resource.p, rt->dsv.resource);
  if (clear) {
    static float color[4] = {0,0,0,0};
    _ctx->ClearRenderTargetView(rt->rtv.resource, color);
    _ctx->ClearDepthStencilView(rt->dsv.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
  }
  _ctx->RSSetViewports(1, &GRAPHICS._viewport);
}

void DeferredContext::set_vs(GraphicsObjectHandle vs) {
  KASSERT(vs.type() == GraphicsObjectHandle::kVertexShader || !vs.is_valid());
  _ctx->VSSetShader(GRAPHICS._vertex_shaders.get(vs), NULL, 0);
}

void DeferredContext::set_cs(GraphicsObjectHandle cs) {
  KASSERT(cs.type() == GraphicsObjectHandle::kComputeShader || !cs.is_valid());
  _ctx->CSSetShader(cs.is_valid() ? GRAPHICS._compute_shaders.get(cs) : NULL, NULL, 0);
}

void DeferredContext::set_gs(GraphicsObjectHandle gs) {
  KASSERT(gs.type() == GraphicsObjectHandle::kGeometryShader || !gs.is_valid());
  _ctx->GSSetShader(gs.is_valid() ? GRAPHICS._geometry_shaders.get(gs) : NULL, NULL, 0);
}

void DeferredContext::set_ps(GraphicsObjectHandle ps) {
  KASSERT(ps.type() == GraphicsObjectHandle::kPixelShader || !ps.is_valid());
  _ctx->PSSetShader(ps.is_valid() ? GRAPHICS._pixel_shaders.get(ps) : NULL, NULL, 0);
}

void DeferredContext::set_layout(GraphicsObjectHandle layout) {
  _ctx->IASetInputLayout(GRAPHICS._input_layouts.get(layout));
}

void DeferredContext::set_vb(ID3D11Buffer *buf, uint32_t stride)
{
  UINT ofs[] = { 0 };
  ID3D11Buffer* bufs[] = { buf };
  uint32_t strides[] = { stride };
  _ctx->IASetVertexBuffers(0, 1, bufs, strides, ofs);
}

void DeferredContext::set_vb(GraphicsObjectHandle vb) {
set_vb(GRAPHICS._vertex_buffers.get(vb), vb.data());
}

void DeferredContext::set_ib(GraphicsObjectHandle ib) {
  _ctx->IASetIndexBuffer(GRAPHICS._index_buffers.get(ib), (DXGI_FORMAT)ib.data(), 0);
}

void DeferredContext::set_topology(D3D11_PRIMITIVE_TOPOLOGY top) {
  _ctx->IASetPrimitiveTopology(top);
}

void DeferredContext::set_rs(GraphicsObjectHandle rs) {
  _ctx->RSSetState(GRAPHICS._rasterizer_states.get(rs));
}

void DeferredContext::set_dss(GraphicsObjectHandle dss, UINT stencil_ref) {
  _ctx->OMSetDepthStencilState(GRAPHICS._depth_stencil_states.get(dss), stencil_ref);
}

void DeferredContext::set_bs(GraphicsObjectHandle bs, const float *blend_factors, UINT sample_mask) {
  _ctx->OMSetBlendState(GRAPHICS._blend_states.get(bs), blend_factors, sample_mask);
}

void DeferredContext::set_samplers(const SamplerArray &samplers) {
  int size = samplers.size() * sizeof(GraphicsObjectHandle);
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
  }
}

void DeferredContext::unset_uavs(int first, int count) {
  UINT initialCount = -1;
  static ID3D11UnorderedAccessView *nullViews[MAX_TEXTURES] = {0, 0, 0, 0, 0, 0, 0, 0};
  _ctx->CSSetUnorderedAccessViews(first, count, nullViews, &initialCount);
}

void DeferredContext::set_uavs(const TextureArray &uavs) {

  int size = uavs.size() * sizeof(GraphicsObjectHandle);
  ID3D11UnorderedAccessView *d3dUavs[MAX_TEXTURES];
  int first_resource = MAX_TEXTURES, num_resources = 0;

  for (int i = 0; i < MAX_TEXTURES; ++i) {

    if (uavs[i].is_valid()) {
      GraphicsObjectHandle h = uavs[i];
      auto type = h.type();

      if (type == GraphicsObjectHandle::kStructuredBuffer) {
        auto *data = GRAPHICS._structured_buffers.get(h);
        d3dUavs[i] = data->uav.resource;
      } else if (type == GraphicsObjectHandle::kRenderTarget) {
        auto *data = GRAPHICS._render_targets.get(h);
        d3dUavs[i] = data->uav.resource;
      } else {
        LOG_ERROR_LN("Trying to set an unsupported UAV type!");
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
      while (uavs[ofs].is_valid()) {
        ofs++;
        cur++;
        if (ofs == MAX_TEXTURES)
          break;
      }
      UINT initialCount = 0;
      _ctx->CSSetUnorderedAccessViews(tmp, cur, &d3dUavs[tmp], &initialCount);
      num_resources_set += cur;
      while (ofs < MAX_TEXTURES && !uavs[ofs].is_valid())
        ofs++;
    }
  }
}

void DeferredContext::set_shader_resource(GraphicsObjectHandle resource, ShaderType::Enum type) {
  TextureArray arr = { resource };
  return set_shader_resources(arr, type);
}

void DeferredContext::set_shader_resources(const TextureArray &resources, ShaderType::Enum type) {
  int size = resources.size() * sizeof(GraphicsObjectHandle);
  ID3D11ShaderResourceView *d3dresources[MAX_TEXTURES];
  int first_resource = MAX_TEXTURES, num_resources = 0;
  for (int i = 0; i < MAX_TEXTURES; ++i) {
    if (resources[i].is_valid()) {
      GraphicsObjectHandle h = resources[i];
      auto type = h.type();
      if (type == GraphicsObjectHandle::kTexture) {
        auto *data = GRAPHICS._textures.get(h);
        d3dresources[i] = data->view.resource;
      } else if (type == GraphicsObjectHandle::kResource) {
        auto *data = GRAPHICS._resources.get(h);
        d3dresources[i] = data->view.resource;
      } else if (type == GraphicsObjectHandle::kRenderTarget) {
        auto *data = GRAPHICS._render_targets.get(h);
        d3dresources[i] = data->srv.resource;
      } else if (type == GraphicsObjectHandle::kStructuredBuffer) {
        auto *data = GRAPHICS._structured_buffers.get(h);
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

      if (type == ShaderType::kVertexShader)
        _ctx->VSSetShaderResources(tmp, cur, &d3dresources[tmp]);
      else if (type == ShaderType::kPixelShader)
        _ctx->PSSetShaderResources(tmp, cur, &d3dresources[tmp]);
      else if (type == ShaderType::kComputeShader)
        _ctx->CSSetShaderResources(tmp, cur, &d3dresources[tmp]);
      else if (type == ShaderType::kGeometryShader)
        _ctx->GSSetShaderResources(tmp, cur, &d3dresources[tmp]);
      else
        LOG_ERROR_LN("Implement me!");

      num_resources_set += cur;
      while (ofs < MAX_TEXTURES && !resources[ofs].is_valid())
        ofs++;
    }
  }
}

void DeferredContext::unset_render_targets(int first, int count) {
  static ID3D11RenderTargetView *nullViews[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  _ctx->OMSetRenderTargets(8, nullViews, nullptr);
}

void DeferredContext::unset_shader_resource(int first_view, int num_views, ShaderType::Enum type) {
  if (!num_views)
    return;
  static ID3D11ShaderResourceView *null_views[MAX_SAMPLERS] = {0, 0, 0, 0, 0, 0, 0, 0};
  if (type == ShaderType::kVertexShader)
    _ctx->VSSetShaderResources(first_view, num_views, null_views);
  else if (type == ShaderType::kPixelShader)
    _ctx->PSSetShaderResources(first_view, num_views, null_views);
  else if (type == ShaderType::kComputeShader)
    _ctx->CSSetShaderResources(first_view, num_views, null_views);
  else if (type == ShaderType::kGeometryShader)
    _ctx->GSSetShaderResources(first_view, num_views, null_views);
  else
    LOG_ERROR_LN("Implement me!");

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
  int firstVsSlot = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
  for (size_t i = 0; i < vs.size(); ++i) {
    if (auto *cur = vs[i]) {
      int slot = cur->slot;
      firstVsSlot = min(firstVsSlot, slot);
      ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(cur->handle);
      D3D11_MAPPED_SUBRESOURCE sub;
      _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
      memcpy(sub.pData, cur->staging.data(), cur->staging.size());
      _ctx->Unmap(buffer, 0);
      vs_cb[slot] = buffer;
    }
  }

  // Copy the ps cbuffers
  int firstPsSlot = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
  for (size_t i = 0; i < ps.size(); ++i) {
    if (auto *cur = ps[i]) {
      int slot = cur->slot;
      firstPsSlot = min(firstPsSlot, slot);
      ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(cur->handle);
      D3D11_MAPPED_SUBRESOURCE sub;
      _ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
      memcpy(sub.pData, cur->staging.data(), cur->staging.size());
      _ctx->Unmap(buffer, 0);
      ps_cb[slot] = buffer;
    }
  }

  if (!vs.empty())
    _ctx->VSSetConstantBuffers(firstVsSlot, vs.size() - firstVsSlot, &vs_cb[firstVsSlot]);

  if (!ps.empty())
      _ctx->PSSetConstantBuffers(firstPsSlot, ps.size() - firstPsSlot, &ps_cb[firstPsSlot]);
}

void DeferredContext::set_cbuffer(GraphicsObjectHandle cb, int slot, ShaderType::Enum type, const void *data, int dataLen) {

  ID3D11Buffer *buffer = GRAPHICS._constant_buffers.get(cb);
  if (!buffer)
    return;
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
  else if (type == ShaderType::kGeometryShader)
    _ctx->GSSetConstantBuffers(slot, 1, &buffer);
  else
    LOG_ERROR_LN("Implement me!");
}

bool DeferredContext::map(GraphicsObjectHandle h, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *res) {
  switch (h.type()) {
  case GraphicsObjectHandle::kTexture:
    return SUCCEEDED(_ctx->Map(GRAPHICS._textures.get(h)->texture.resource, sub, type, flags, res));

  case GraphicsObjectHandle::kVertexBuffer:
    return SUCCEEDED(_ctx->Map(GRAPHICS._vertex_buffers.get(h), sub, type, flags, res));

  case GraphicsObjectHandle::kIndexBuffer:
    return SUCCEEDED(_ctx->Map(GRAPHICS._index_buffers.get(h), sub, type, flags, res));

  default:
    LOG_ERROR_LN("Invalid resource type passed to %s", __FUNCTION__);
    return false;
  }
}

void DeferredContext::unmap(GraphicsObjectHandle h, UINT sub) {
  switch (h.type()) {
  case GraphicsObjectHandle::kTexture:
    _ctx->Unmap(GRAPHICS._textures.get(h)->texture.resource, sub);
    break;

  case GraphicsObjectHandle::kVertexBuffer:
    _ctx->Unmap(GRAPHICS._vertex_buffers.get(h), sub);
    break;

  case GraphicsObjectHandle::kIndexBuffer:
    _ctx->Unmap(GRAPHICS._index_buffers.get(h), sub);
    break;

  default:
    LOG_WARNING_LN("Invalid resource type passed to %s", __FUNCTION__);
  }
}

void DeferredContext::draw_indexed(int count, int start_index, int base_vertex) {
  _ctx->DrawIndexed(count, start_index, base_vertex);
}

void DeferredContext::draw(int vertexCount, int startVertexLocation) {
  _ctx->Draw(vertexCount, startVertexLocation);
}

void DeferredContext::dispatch(int threadGroupCountX, int threadGroupCountY, int threadGroupCountZ) {
  _ctx->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void DeferredContext::begin_frame() {
}

void DeferredContext::end_frame() {
  if (!_is_immediate_context) {
    ID3D11CommandList *cmd_list;
    _ctx->FinishCommandList(FALSE, &cmd_list);
    GRAPHICS.add_command_list(cmd_list);
  }
}


