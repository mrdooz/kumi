#include "stdafx.h"
#include "renderer.hpp"
#include "graphics.hpp"
#include "technique.hpp"
#include "material_manager.hpp"
#include "FW1FontWrapper.h"

using namespace std;

static const int cEffectDataSize = 1024 * 1024;


Renderer *Renderer::_instance;

Renderer::Renderer()
  : _effect_data(new uint8[cEffectDataSize])
  , _effect_data_ofs(0)
{
}

bool Renderer::create() {
  assert(!_instance);
  _instance = new Renderer();
  return true;
}

bool Renderer::close() {
  assert(_instance);
  delete exch_null(_instance);
  return true;
}

Renderer &Renderer::instance() {
  assert(_instance);
  return *_instance;
}

void Renderer::render() {

  ID3D11DeviceContext *ctx = GRAPHICS.context();

  Graphics::BackedResources *res = GRAPHICS.get_backed_resources();

  ctx->RSSetViewports(1, &GRAPHICS.viewport());

  // i need to think about how i want to do this sorting, how to group stuff etc, so at the moment i'm just
  // leaving it off..
  // sort by keys
  //sort(begin(_render_commands), end(_render_commands), 
//    [&](const RenderCmd &a, const RenderCmd &b) { return a.key.data < b.key.data; });

  // delete commands are sorted before render commands, so we can just save the
  // deleted items when we find them
  set<void *> deleted_items;

  for (size_t i = 0; i < _render_commands.size(); ++i) {
    const RenderCmd &cmd = _render_commands[i];
    RenderKey key = cmd.key;
    void *data = cmd.data;

    if (deleted_items.find(data) != deleted_items.end())
      continue;

    switch (key.cmd) {

      case RenderKey::kDelete:
        deleted_items.insert(data);
        break;

      case RenderKey::kSetRenderTarget: {
        GraphicsObjectHandle h = key.handle;
        if (RenderTargetData *rt = res->_render_targets.get(h)) {
          ctx->OMSetRenderTargets(1, &(rt->rtv.p), rt->dsv);
          if (data) {
            const XMFLOAT4 &f = *(XMFLOAT4 *)cmd.data;
            GRAPHICS.clear(h, f);
          }
        }
        break;
      }

      case RenderKey::kRenderTechnique: {

        TechniqueRenderData *d = (TechniqueRenderData *)data;
        Technique *technique = res->_techniques.get(d->technique);

        Shader *vertex_shader = technique->vertex_shader();
        Shader *pixel_shader = technique->pixel_shader();

        ctx->VSSetShader(res->_vertex_shaders.get(vertex_shader->handle()), NULL, 0);
        ctx->GSSetShader(NULL, 0, 0);
        ctx->PSSetShader(res->_pixel_shaders.get(pixel_shader->handle()), NULL, 0);

        GRAPHICS.set_vb(ctx, res->_vertex_buffers.get(technique->vb()), technique->vertex_size());
        ctx->IASetIndexBuffer(res->_index_buffers.get(technique->ib()), technique->index_format(), 0);
        ctx->IASetInputLayout(res->_input_layouts.get(technique->input_layout()));

        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ctx->RSSetState(res->_rasterizer_states.get(technique->rasterizer_state()));
        ctx->OMSetDepthStencilState(res->_depth_stencil_states.get(technique->depth_stencil_state()), ~0);
        ctx->OMSetBlendState(res->_blend_states.get(technique->blend_state()), GRAPHICS.default_blend_factors(), 
          GRAPHICS.default_sample_mask());

        GRAPHICS.set_cbuffer_params(technique, vertex_shader, -1, -1);
        GRAPHICS.set_cbuffer_params(technique, pixel_shader, -1, -1);
        GRAPHICS.set_samplers(technique, pixel_shader);
        int resource_mask = 0;
        GRAPHICS.set_resource_views(technique, pixel_shader, &resource_mask);

        const std::string &sampler_name = technique->get_default_sampler_state();
        if (!sampler_name.empty()) {
          GraphicsObjectHandle sampler = GRAPHICS.get_sampler_state(technique->name().c_str(), sampler_name.c_str());
          ID3D11SamplerState *samplers = res->_sampler_states.get(sampler);
          ctx->PSSetSamplers(0, 1, &samplers);
        }
/*
        int num_textures = 0;
        ID3D11ShaderResourceView *textures[8];
        for (int i = 0; d->textures[i].is_valid(); ++i, ++num_textures)
          textures[i] = res->_resources.get(d->textures[i])->srv;

        if (num_textures)
          ctx->PSSetShaderResources(0, num_textures, textures);

        int num_rendertargets = 0;
        ID3D11ShaderResourceView *render_targets[8];
        for (int i = 0; d->render_targets[i].is_valid(); ++i, ++num_rendertargets)
          render_targets[i] = res->_render_targets.get(d->render_targets[i])->srv;

        if (num_rendertargets)
          ctx->PSSetShaderResources(0, num_rendertargets, render_targets);
*/
        ctx->DrawIndexed(technique->index_count(), 0, 0);
        GRAPHICS.unbind_resource_views(resource_mask);

        break;
      }

      case RenderKey::kRenderMesh: {
        MeshRenderData *render_data = (MeshRenderData *)data;
        const uint16 material_id = render_data->material_id;
        const Material *material = MATERIAL_MANAGER.get_material(material_id);
        Technique *technique = res->_techniques.get(render_data->technique);
        Shader *vertex_shader = technique->vertex_shader();
        Shader *pixel_shader = technique->pixel_shader();

        ctx->VSSetShader(res->_vertex_shaders.get(vertex_shader->handle()), NULL, 0);
        ctx->GSSetShader(NULL, 0, 0);
        ctx->PSSetShader(res->_pixel_shaders.get(pixel_shader->handle()), NULL, 0);

        GRAPHICS.set_vb(ctx, res->_vertex_buffers.get(render_data->vb), render_data->vertex_size);
        ctx->IASetIndexBuffer(res->_index_buffers.get(render_data->ib), render_data->index_format, 0);
        ctx->IASetInputLayout(res->_input_layouts.get(technique->input_layout()));

        ctx->IASetPrimitiveTopology(render_data->topology);

        ctx->RSSetState(res->_rasterizer_states.get(technique->rasterizer_state()));
        ctx->OMSetDepthStencilState(res->_depth_stencil_states.get(technique->depth_stencil_state()), ~0);
        ctx->OMSetBlendState(res->_blend_states.get(technique->blend_state()), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

        GRAPHICS.set_cbuffer_params(technique, vertex_shader, material_id, render_data->mesh_id);
        GRAPHICS.set_cbuffer_params(technique, pixel_shader, material_id, render_data->mesh_id);
        GRAPHICS.set_samplers(technique, pixel_shader);
        int resource_mask;
        GRAPHICS.set_resource_views(technique, pixel_shader, &resource_mask);

        ctx->DrawIndexed(render_data->index_count, 0, 0);
        GRAPHICS.unbind_resource_views(resource_mask);
        break;
      }

      case RenderKey::kRenderBuffers: {
        BufferRenderData *render_data = (BufferRenderData *)data;

        ctx->VSSetShader(res->_vertex_shaders.get(render_data->vs), NULL, 0);
        ctx->GSSetShader(NULL, 0, 0);
        ctx->PSSetShader(res->_pixel_shaders.get(render_data->ps), NULL, 0);

        GRAPHICS.set_vb(ctx, res->_vertex_buffers.get(render_data->vb), render_data->vertex_size);
        bool indexed = render_data->ib.is_valid();
        if (indexed) ctx->IASetIndexBuffer(res->_index_buffers.get(render_data->ib), render_data->index_format, 0);
        ctx->IASetInputLayout(res->_input_layouts.get(render_data->layout));

        ctx->IASetPrimitiveTopology(render_data->topology);

        if (render_data->rs.is_valid()) ctx->RSSetState(res->_rasterizer_states.get(render_data->rs));
        if (render_data->dss.is_valid()) ctx->OMSetDepthStencilState(res->_depth_stencil_states.get(render_data->dss), ~0);
        if (render_data->bs.is_valid()) ctx->OMSetBlendState(res->_blend_states.get(render_data->bs), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

        ID3D11SamplerState *samplers[8];
        int num_samplers = 0;
        for (int i = 0; render_data->samplers[i].is_valid(); ++i, ++num_samplers)
          samplers[i] = res->_sampler_states.get(render_data->samplers[i]);

        if (num_samplers)
          ctx->PSSetSamplers(0, num_samplers, samplers);

        int num_textures = 0;
        ID3D11ShaderResourceView *textures[8];
        for (int i = 0; render_data->textures[i].is_valid(); ++i, ++num_textures)
          textures[i] = res->_resources.get(render_data->textures[i])->srv;

        if (num_textures)
          ctx->PSSetShaderResources(0, num_textures, textures);

        if (indexed)
          ctx->DrawIndexed(render_data->index_count, render_data->start_index, render_data->base_vertex);
        else
          ctx->Draw(render_data->vertex_count, render_data->start_vertex);
        break;
      }

      case RenderKey::kRenderText: {
        TextRenderData *rd = (TextRenderData *)data;
        IFW1FontWrapper *wrapper = res->_font_wrappers.get(rd->font);
        wrapper->DrawString(GRAPHICS.context(), rd->str, rd->font_size, rd->x, rd->y, rd->color, 0);
        break;
      }
    }
  }

  _render_commands.clear();
  _effect_data_ofs = 0;
}

void *Renderer::strdup(const char *str) {
  int len = strlen(str);
  void *mem = malloc(len + 1);
  memcpy(mem, str, len);
  return mem;
}

void *Renderer::raw_alloc(size_t size) {
  if (size + _effect_data_ofs >= cEffectDataSize)
    return nullptr;

  void *ptr = &_effect_data.get()[_effect_data_ofs];
  _effect_data_ofs += size;
  return ptr;
}

void Renderer::validate_command(RenderKey key, const void *data) {

  Graphics::BackedResources *res = GRAPHICS.get_backed_resources();

  switch (key.cmd) {

    case RenderKey::kSetRenderTarget:
      break;

    case RenderKey::kRenderMesh: {
      MeshRenderData *render_data = (MeshRenderData *)data;
      const uint16 material_id = render_data->material_id;
      const Material *material = MATERIAL_MANAGER.get_material(material_id);
      Technique *technique = res->_techniques.get(render_data->technique);
      assert(technique);
      Shader *vertex_shader = technique->vertex_shader();
      assert(vertex_shader);
      Shader *pixel_shader = technique->pixel_shader();
      assert(pixel_shader);
      break;
    }

    case RenderKey::kRenderTechnique:
      break;
  }
}

void Renderer::submit_command(const TrackedLocation &location, RenderKey key, void *data) {
#if _DEBUG
  validate_command(key, data);
#endif
  _render_commands.push_back(RenderCmd(location, key, data));
}

uint16 Renderer::next_seq_nr() const { 
  assert(_render_commands.size() < 65536); 
  return (uint16)_render_commands.size(); 
}

void Renderer::submit_technique(GraphicsObjectHandle technique) {
  RenderKey key;
  key.cmd = RenderKey::kRenderTechnique;
  TechniqueRenderData *data = RENDERER.alloc_command_data<TechniqueRenderData>();
  data->technique = technique;
  RENDERER.submit_command(FROM_HERE, key, data);
}
