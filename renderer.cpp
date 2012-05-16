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
  _cmd_buffer.resize(1024 * 1024);
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

enum RenderOp {
  kOpSetVS,
  kOpSetGS,
  kOpSetPS,
  kOpSetVB,
  kOpSetIB,
  kOpSetCBuffer,
  kOpSetLayout,
  kOpSetTopology,
  kOpSetRS,
  kOpSetDSS,
  kOpSetBS,
  kOpSetRenderTarget,
  kOpDrawIndexed,
};

#define PUSH_CMD(type, value) *(type *)buf++ = (value);

class CmdGen {
public:
  CmdGen(uint8 *buf, Graphics::BackedResources *res) : buf(buf), org_buf(buf), res(res) {}

  void add_set_vs(GraphicsObjectHandle vs) {
    if (prev_vs != vs)
      push_cmd(kOpSetVS, res->_vertex_shaders.get(vs));
    prev_vs = vs;
  }

  void add_set_ps(GraphicsObjectHandle ps) {
    if (prev_ps != ps)
      push_cmd(kOpSetPS, res->_pixel_shaders.get(ps));
    prev_ps = ps;
  }

  void add_set_layout(GraphicsObjectHandle layout) {
    if (prev_layout != layout)
      push_cmd(kOpSetLayout, res->_input_layouts.get(layout));
    prev_layout = layout;
  }

  void add_set_vb(GraphicsObjectHandle vb, int vertex_size) {
    if (prev_vb != vb)
      push_cmd(kOpSetVB, res->_vertex_buffers.get(vb), vertex_size);
    prev_vb = vb;
  }

  void add_set_ib(GraphicsObjectHandle ib, DXGI_FORMAT format) {
    if (prev_ib != ib)
      push_cmd(kOpSetIB, res->_index_buffers.get(ib), format);
    prev_ib = ib;
  }

  void add_set_topology(D3D11_PRIMITIVE_TOPOLOGY top) {
    if (prev_topology != top)
      push_cmd(kOpSetTopology, top);
    prev_topology = top;
  }

  void add_set_rs(GraphicsObjectHandle rs) {
    if (prev_rs != rs)
      push_cmd(kOpSetRS, res->_rasterizer_states.get(rs));
    prev_rs = rs;
  }

  void add_set_dss(GraphicsObjectHandle dss, UINT stencil_mask) {
    if (prev_dss != dss)
      push_cmd(kOpSetDSS, res->_depth_stencil_states.get(dss), stencil_mask);
    prev_dss = dss;
  }

  void add_set_bs(GraphicsObjectHandle bs, float *blend_factors, UINT sample_mask) {
    if (prev_bs != bs)
      push_cmd(kOpSetBS, res->_blend_states.get(bs), blend_factors[0], blend_factors[1], blend_factors[2], blend_factors[3], sample_mask);
    prev_bs = bs;
  }

  void add_set_cbuffer(ID3D11Buffer *buffer, void *data, int len) {
    push_cmd(kOpSetCBuffer, buffer, len);
    memcpy(buf, data, len);
    buf += len;
  }

  void add_set_cbuffer(ID3D11Buffer *buffer) {
    push_cmd(kOpSetCBuffer, buffer);
  }

  void add_draw_indexed(int count, int start_index, int base_vertex) {
    push_cmd(kOpDrawIndexed, count, start_index, base_vertex);
  }

  int get_cmd_size() const {
    return buf - org_buf;
  }

private:

  template<typename T>
  void push_cmd(const T &t) {
    *(T *)buf = t; buf += sizeof(T);
  }

  template<typename T, typename V1>
  void push_cmd(const T &t, const V1 &v1) {
    push_cmd(t);
    *(V1 *)buf = v1; buf += sizeof(V1);
  }

  template<typename T, typename V1, typename V2>
  void push_cmd(const T &t, const V1 &v1, const V2 &v2) {
    push_cmd(t, v1);
    *(V2 *)buf = v2; buf += sizeof(V2);
  }

  template<typename T, typename V1, typename V2, typename V3>
  void push_cmd(const T &t, const V1 &v1, const V2 &v2, const V3 &v3) {
    push_cmd(t, v1, v2);
    *(V3 *)buf = v3; buf += sizeof(V3);
  }

  template<typename T, typename V1, typename V2, typename V3, typename V4>
  void push_cmd(const T &t, const V1 &v1, const V2 &v2, const V3 &v3, const V4 &v4) {
    push_cmd(t, v1, v2, v3);
    *(V4 *)buf = v4; buf += sizeof(V4);
  }

  template<typename T, typename V1, typename V2, typename V3, typename V4, typename V5>
  void push_cmd(const T &t, const V1 &v1, const V2 &v2, const V3 &v3, const V4 &v4, const V5 &v5) {
    push_cmd(t, v1, v2, v3, v4);
    *(V5 *)buf = v5; buf += sizeof(V5);
  }

  template<typename T, typename V1, typename V2, typename V3, typename V4, typename V5, typename V6>
  void push_cmd(const T &t, const V1 &v1, const V2 &v2, const V3 &v3, const V4 &v4, const V5 &v5, const V6 &v6) {
    push_cmd(t, v1, v2, v3, v4, v5);
    *(V6 *)buf = v6; buf += sizeof(V6);
  }

  uint8 *buf;
  uint8 *org_buf;
  Graphics::BackedResources *res;

  GraphicsObjectHandle prev_vs, prev_ps, prev_layout;
  GraphicsObjectHandle prev_rs, prev_bs, prev_dss;
  GraphicsObjectHandle prev_ib, prev_vb;
  D3D11_PRIMITIVE_TOPOLOGY prev_topology;
};

template<typename T>
T read_advance(uint8 **buf) {
  T tmp = *((T *)*buf);
  *buf += sizeof(T);
  return tmp;
}

void read_advance_raw(void *out, int len, uint8 **buf) {
  memcpy(out, *buf, len);
  *buf += len;
}

void Renderer::render() {

  ID3D11DeviceContext *ctx = GRAPHICS.context();

  Graphics::BackedResources *res = GRAPHICS.get_backed_resources();

  ctx->RSSetViewports(1, &GRAPHICS.viewport());

  // sort by keys (just using the depth)
  stable_sort(begin(_render_commands), end(_render_commands), 
    [&](const RenderCmd &a, const RenderCmd &b) { return (a.key.data & 0xffff000000000000) < (b.key.data & 0xffff000000000000); });
#if 1
  CmdGen gen(&_cmd_buffer[0], res);
  // build the cmd buffer from the render commands
  for (auto it = begin(_render_commands), e = end(_render_commands); it != e; ++it) {
    const RenderCmd &cmd = *it;
    RenderKey key = cmd.key;
    void *data = cmd.data;

    switch (key.cmd) {

      case RenderKey::kRenderMesh: {
        MeshRenderData *render_data = (MeshRenderData *)data;
        Technique *technique = res->_techniques.get(render_data->technique);
        RenderObjects objects;
        technique->get_render_objects(&objects);
        gen.add_set_vs(objects.vs);
        gen.add_set_ps(objects.ps);

        gen.add_set_vb(render_data->vb, render_data->vertex_size);
        gen.add_set_layout(objects.layout);
        gen.add_set_ib(render_data->ib, render_data->index_format);

        gen.add_set_topology(render_data->topology);
        gen.add_set_rs(objects.rs);
        gen.add_set_dss(objects.dss, objects.stencil_ref);
        gen.add_set_bs(objects.bs, objects.blend_factors, objects.sample_mask);

        GraphicsObjectHandle cb = technique->get_cbuffers()[0].handle;
        ID3D11Buffer *buffer = res->_constant_buffers.get(cb);
        gen.add_set_cbuffer(buffer, render_data->cbuffer_staged, render_data->cbuffer_len);
        gen.add_draw_indexed(render_data->index_count, 0, 0);
        break;
      }
    }
  }

  uint8 *cur = &_cmd_buffer[0];
  uint8 *end = cur + gen.get_cmd_size();

  while (cur < end) {
    RenderOp op = read_advance<RenderOp>(&cur);
    switch (op) {

      case kOpSetVS:
        ctx->VSSetShader(read_advance<ID3D11VertexShader *>(&cur), NULL, 0);
        break;

      case kOpSetPS:
        ctx->PSSetShader(read_advance<ID3D11PixelShader *>(&cur), NULL, 0);
        break;

      case kOpSetVB: {
        ID3D11Buffer *buf = read_advance<ID3D11Buffer *>(&cur);
        int vertex_size = read_advance<int>(&cur);
        GRAPHICS.set_vb(ctx, buf, vertex_size);
        break;
      }

      case kOpSetIB: {
        ID3D11Buffer *buf = read_advance<ID3D11Buffer *>(&cur);
        DXGI_FORMAT fmt = read_advance<DXGI_FORMAT>(&cur);
        ctx->IASetIndexBuffer(buf, fmt, 0);
        break;
      }

      case kOpSetLayout:
        ctx->IASetInputLayout(read_advance<ID3D11InputLayout *>(&cur));
        break;

      case kOpSetTopology:
        ctx->IASetPrimitiveTopology(read_advance<D3D11_PRIMITIVE_TOPOLOGY>(&cur));
        break;

      case kOpSetRS:
        ctx->RSSetState(read_advance<ID3D11RasterizerState *>(&cur));
        break;

      case kOpSetBS: {
        ID3D11BlendState *bs = read_advance<ID3D11BlendState *>(&cur);
        float blend_factors[4];
        read_advance_raw(blend_factors, sizeof(blend_factors), &cur);
        UINT sample_mask = read_advance<UINT>(&cur);
        ctx->OMSetBlendState(bs, blend_factors, sample_mask);
        break;
      }

      case kOpSetDSS: {
        ID3D11DepthStencilState *dss = read_advance<ID3D11DepthStencilState *>(&cur);
        UINT stencil_ref = read_advance<UINT>(&cur);
        ctx->OMSetDepthStencilState(dss, stencil_ref);
        break;
      }

      case kOpSetCBuffer: {
        ID3D11Buffer *buffer = read_advance<ID3D11Buffer *>(&cur);
        int len = read_advance<int>(&cur);
        void *data = _alloca(len);
        read_advance_raw(data, len, &cur);

        D3D11_MAPPED_SUBRESOURCE sub;
        ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
        memcpy(sub.pData, data, len);
        ctx->Unmap(buffer, 0);
        ctx->VSSetConstantBuffers(0, 1, &buffer);
        ctx->PSSetConstantBuffers(0, 1, &buffer);
        break;
      }

      case kOpDrawIndexed: {
        UINT count = read_advance<UINT>(&cur);
        UINT start_index = read_advance<UINT>(&cur);
        UINT base_vertex = read_advance<UINT>(&cur);
        ctx->DrawIndexed(count, start_index, base_vertex);
        break;
      }
    }
  }
#else
  for (size_t i = 0; i < _render_commands.size(); ++i) {
    const RenderCmd &cmd = _render_commands[i];
    RenderKey key = cmd.key;
    void *data = cmd.data;

    switch (key.cmd) {

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

      case RenderKey::kRenderMesh: {
        MeshRenderData *render_data = (MeshRenderData *)data;
        Technique *technique = res->_techniques.get(render_data->technique);
        RenderObjects objects;
        technique->get_render_objects(&objects);
        //Shader *vertex_shader = technique->vertex_shader();
        //Shader *pixel_shader = technique->pixel_shader();

        ctx->VSSetShader(res->_vertex_shaders.get(objects.vs), NULL, 0);
        ctx->GSSetShader(NULL, 0, 0);
        ctx->PSSetShader(res->_pixel_shaders.get(objects.ps), NULL, 0);

        GRAPHICS.set_vb(ctx, res->_vertex_buffers.get(render_data->vb), render_data->vertex_size);
        ctx->IASetIndexBuffer(res->_index_buffers.get(render_data->ib), render_data->index_format, 0);
        ctx->IASetInputLayout(res->_input_layouts.get(objects.layout));

        ctx->IASetPrimitiveTopology(render_data->topology);

        ctx->RSSetState(res->_rasterizer_states.get(objects.rs));
        ctx->OMSetDepthStencilState(res->_depth_stencil_states.get(objects.dss), ~0);
        ctx->OMSetBlendState(res->_blend_states.get(objects.bs), objects.blend_factors, objects.sample_mask);

        GraphicsObjectHandle cb = technique->get_cbuffers()[0].handle;
        ID3D11Buffer *buffer = res->_constant_buffers.get(cb);
        D3D11_MAPPED_SUBRESOURCE sub;
        ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
        memcpy(sub.pData, render_data->cbuffer_staged, render_data->cbuffer_len);
        ctx->Unmap(buffer, 0);
        ctx->VSSetConstantBuffers(0, 1, &buffer);
        ctx->PSSetConstantBuffers(0, 1, &buffer);
/*
        GRAPHICS.set_samplers(technique, pixel_shader);
        int resource_mask;
        GRAPHICS.set_resource_views(technique, pixel_shader, &resource_mask);
*/
        ctx->DrawIndexed(render_data->index_count, 0, 0);
        //GRAPHICS.unbind_resource_views(resource_mask);
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

        //GRAPHICS.set_cbuffer_params(technique, vertex_shader, -1, -1);
        //GRAPHICS.set_cbuffer_params(technique, pixel_shader, -1, -1);
/*
        GraphicsObjectHandle cb = technique->get_cbuffers()[0].handle;
        ID3D11Buffer *buffer = res->_constant_buffers.get(cb);
        D3D11_MAPPED_SUBRESOURCE sub;
        ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
        memcpy(sub.pData, render_data->cbuffer_staged, render_data->cbuffer_len);
        ctx->Unmap(buffer, 0);
        ctx->VSSetConstantBuffers(0, 1, &buffer);
        ctx->PSSetConstantBuffers(0, 1, &buffer);
*/
        GRAPHICS.set_samplers(technique, pixel_shader);
        int resource_mask = 0;
        GRAPHICS.set_resource_views(technique, pixel_shader, &resource_mask);

        const std::string &sampler_name = technique->get_default_sampler_state();
        if (!sampler_name.empty()) {
          GraphicsObjectHandle sampler = GRAPHICS.get_sampler_state(technique->name().c_str(), sampler_name.c_str());
          ID3D11SamplerState *samplers = res->_sampler_states.get(sampler);
          ctx->PSSetSamplers(0, 1, &samplers);
        }

        ctx->DrawIndexed(technique->index_count(), 0, 0);
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
#if WITH_GWEN
        TextRenderData *rd = (TextRenderData *)data;
        IFW1FontWrapper *wrapper = res->_font_wrappers.get(rd->font);
        wrapper->DrawString(GRAPHICS.context(), rd->str, rd->font_size, rd->x, rd->y, rd->color, 0);
#else
        assert(!"Compile with WITH_GWEN to get font rendering to work");
#endif
        break;
      }
    }
  }
#endif
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
      //const uint16 material_id = render_data->material_id;
      //const Material *material = MATERIAL_MANAGER.get_material(material_id);
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
