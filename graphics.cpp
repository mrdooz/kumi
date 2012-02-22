#include "stdafx.h"
#include "graphics.hpp"
#include "logger.hpp"
#include "technique.hpp"
#include "technique_parser.hpp"
#include "resource_interface.hpp"
#include "property_manager.hpp"
#include "material_manager.hpp"

using namespace std;
using namespace std::tr1::placeholders;

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "DXGUID.lib")
#pragma comment(lib, "D3D11.lib")

namespace
{
  HRESULT create_buffer_inner(ID3D11Device* device, const D3D11_BUFFER_DESC& desc, const void* data, ID3D11Buffer** buffer)
  {
    D3D11_SUBRESOURCE_DATA init_data;
    ZeroMemory(&init_data, sizeof(init_data));
    init_data.pSysMem = data;
    return device->CreateBuffer(&desc, data ? &init_data : NULL, buffer);
  }
}



Graphics* Graphics::_instance = NULL;

Graphics& Graphics::instance() {
  assert(_instance);
  return *_instance;
}

bool Graphics::create(ResourceInterface *ri) {
  assert(!_instance);
  _instance = new Graphics(ri);
  return true;
}

Graphics::Graphics(ResourceInterface *ri)
  : _width(-1)
  , _height(-1)
  , _start_fps_time(0xffffffff)
  , _frame_count(0)
  , _fps(0)
  , _vs_profile("vs_4_0")
  , _ps_profile("ps_4_0")
  , _ri(ri)
{
  assert(!_instance);
}

Graphics::~Graphics()
{
}

HRESULT Graphics::create_static_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, const void* data, ID3D11Buffer** vertex_buffer) 
{
  return create_buffer_inner(_device, CD3D11_BUFFER_DESC(vertex_count * vertex_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE), data, vertex_buffer);
}

HRESULT Graphics::create_static_index_buffer(uint32_t index_count, uint32_t elem_size, const void* data, ID3D11Buffer** index_buffer) {
  return create_buffer_inner(_device, CD3D11_BUFFER_DESC(index_count * elem_size, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_IMMUTABLE), data, index_buffer);
}

HRESULT Graphics::create_dynamic_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, ID3D11Buffer** vertex_buffer)
{
  return create_buffer_inner(_device, 
    CD3D11_BUFFER_DESC(vertex_count * vertex_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), 
    NULL, vertex_buffer);
}

void Graphics::set_vb(ID3D11DeviceContext *context, ID3D11Buffer *buf, uint32_t stride)
{
  UINT ofs[] = { 0 };
  ID3D11Buffer* bufs[] = { buf };
  uint32_t strides[] = { stride };
  context->IASetVertexBuffers(0, 1, bufs, strides, ofs);
}

GraphicsObjectHandle Graphics::create_render_target(int width, int height, bool shader_resource, const char *name) {
  RenderTargetData *data = new RenderTargetData;
  if (!create_render_target(width, height, shader_resource, data)) {
    delete exch_null(data);
    return GraphicsObjectHandle();
  }

  int idx = name ? _res._render_targets.find_by_token(name) : -1;
  if (idx != -1 || (idx = _res._render_targets.find_free_index()) != -1) {
    if (_res._render_targets[idx].first)
      _res._render_targets[idx].first->reset();
    _res._render_targets[idx] = make_pair(data, name);
    return GraphicsObjectHandle(GraphicsObjectHandle::kRenderTarget, 0, idx);
  }
  return GraphicsObjectHandle();
}

bool Graphics::create_render_target(int width, int height, bool shader_resource, RenderTargetData *out)
{
  out->reset();

  // create the render target
  ZeroMemory(&out->texture_desc, sizeof(out->texture_desc));
  out->texture_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, 
    D3D11_BIND_RENDER_TARGET | shader_resource * D3D11_BIND_SHADER_RESOURCE);
  B_WRN_HR(_device->CreateTexture2D(&out->texture_desc, NULL, &out->texture.p));

  // create the render target view
  out->rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
  B_WRN_HR(_device->CreateRenderTargetView(out->texture, &out->rtv_desc, &out->rtv.p));

  // create the depth stencil texture
  out->depth_buffer_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
  B_WRN_HR(_device->CreateTexture2D(&out->depth_buffer_desc, NULL, &out->depth_stencil.p));

  // create depth stencil view
  out->dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT);
  B_WRN_HR(_device->CreateDepthStencilView(out->depth_stencil, &out->dsv_desc, &out->dsv.p));

  if (shader_resource) {
    // create the shader resource view
    out->srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
    B_WRN_HR(_device->CreateShaderResourceView(out->texture, &out->srv_desc, &out->srv.p));
  }

  return true;
}

GraphicsObjectHandle Graphics::create_texture(const D3D11_TEXTURE2D_DESC &desc, const char *name) {
  TextureData *data = new TextureData;
  if (!create_texture(desc, data)) {
    delete exch_null(data);
    return GraphicsObjectHandle();
  }

  int idx = name ? _res._textures.find_by_token(name) : -1;
  if (idx != -1 || (idx = _res._textures.find_free_index()) != -1) {
    if (_res._textures[idx].first)
      _res._textures[idx].first->reset();
    _res._textures[idx] = make_pair(data, name);
    return GraphicsObjectHandle(GraphicsObjectHandle::kTexture, 0, idx);
  }
  return GraphicsObjectHandle();
}

bool Graphics::create_texture(const D3D11_TEXTURE2D_DESC &desc, TextureData *out)
{
  out->reset();

  // create the texture
  ZeroMemory(&out->texture_desc, sizeof(out->texture_desc));
  out->texture_desc = desc;
  B_WRN_HR(_device->CreateTexture2D(&out->texture_desc, NULL, &out->texture.p));

  // create the shader resource view if the texture has a shader resource bind flag
  if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
    out->srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
    B_WRN_HR(_device->CreateShaderResourceView(out->texture, &out->srv_desc, &out->srv.p));
  }

  return true;
}

bool Graphics::create_texture(int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, TextureData *out)
{
  if (!create_texture(CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), out))
    return false;

  D3D11_MAPPED_SUBRESOURCE resource;
  B_ERR_HR(_immediate_context->Map(out->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
  uint8_t *src = (uint8_t *)data;
  uint8_t *dst = (uint8_t *)resource.pData;
  const int w = std::min<int>(width, data_width);
  const int h = std::min<int>(height, data_height);
  for (int i = 0; i < h; ++i) {
    memcpy(dst, src, w);
    src += data_pitch;
    dst += resource.RowPitch;
  }
  _immediate_context->Unmap(out->texture, 0);
  return true;
}

bool Graphics::create_back_buffers(int width, int height)
{
  _width = width;
  _height = height;

  int idx = _res._render_targets.find_by_token("default_rt");
  RenderTargetData *rt = nullptr;
  if (idx != -1) {
    rt = _res._render_targets.get(idx);
    rt->reset();
    // release any existing buffers
    _swap_chain->ResizeBuffers(1, width, height, _buffer_format, 0);
  } else {
    rt = new RenderTargetData;
    idx = _res._render_targets.find_free_index();
  }

  // Get the dx11 back buffer
  B_ERR_HR(_swap_chain->GetBuffer(0, IID_PPV_ARGS(&rt->texture.p)));
  rt->texture->GetDesc(&rt->texture_desc);

  B_ERR_HR(_device->CreateRenderTargetView(rt->texture, NULL, &rt->rtv));
  rt->rtv->GetDesc(&rt->rtv_desc);

  // depth buffer
  B_ERR_HR(_device->CreateTexture2D(
    &CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL), 
    NULL, &rt->depth_stencil.p));

  B_ERR_HR(_device->CreateDepthStencilView(rt->depth_stencil, NULL, &rt->dsv.p));
  rt->dsv->GetDesc(&rt->dsv_desc);

  _res._render_targets[idx] = make_pair(rt, "default_rt");
  _default_rt_handle = GraphicsObjectHandle(GraphicsObjectHandle::kRenderTarget, 0, idx);

  _viewport = CD3D11_VIEWPORT (0.0f, 0.0f, (float)_width, (float)_height);

  set_default_render_target();

  return true;
}

bool Graphics::init(const HWND hwnd, const int width, const int height)
{
  _width = width;
  _height = height;
  _buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM; //DXGI_FORMAT_R8G8B8A8_UNORM;

  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory( &sd, sizeof( sd ) );
  sd.BufferCount = 2;
  sd.BufferDesc.Width = width;
  sd.BufferDesc.Height = height;
  sd.BufferDesc.Format = _buffer_format;
  sd.BufferDesc.RefreshRate.Numerator = 0;
  sd.BufferDesc.RefreshRate.Denominator = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;

  // Create DXGI factory to enumerate adapters
  CComPtr<IDXGIFactory1> dxgi_factory;
  B_ERR_HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&dxgi_factory));

  // Use the first adapter
  vector<CComPtr<IDXGIAdapter1> > adapters;
  UINT i = 0;
  IDXGIAdapter1 *adapter = nullptr;
  int perfhud = -1;
  for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters1(i, &adapter)); ++i) {
    adapters.push_back(adapter);
    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
    if (wcscmp(desc.Description, L"NVIDIA PerfHud") == 0) {
      perfhud = i;
    }
  }

  B_ERR_BOOL(!adapters.empty());

  adapter = perfhud != -1 ? adapters[perfhud] : adapters.front();

#ifdef _DEBUG
  const int flags = D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#else
  const int flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

  // Create the DX11 device
  B_ERR_HR(D3D11CreateDeviceAndSwapChain(
    adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &sd, &_swap_chain, &_device, &_feature_level, &_immediate_context.p));

  B_ERR_BOOL(_feature_level >= D3D_FEATURE_LEVEL_9_3);

  B_ERR_HR(_device->QueryInterface(IID_ID3D11Debug, (void **)&_d3d_debug));

  B_ERR_BOOL(create_back_buffers(width, height));

  B_ERR_HR(_device->CreateDepthStencilState(&CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT()), &_default_depth_stencil_state.p));
  B_ERR_HR(_device->CreateRasterizerState(&CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT()), &_default_rasterizer_state.p));
  B_ERR_HR(_device->CreateBlendState(&CD3D11_BLEND_DESC(CD3D11_DEFAULT()), &_default_blend_state.p));
  for (int i = 0; i < 4; ++i)
    _default_blend_factors[i] = 1.0f;
  return true;
}

void Graphics::set_default_render_target()
{
  RenderTargetData *rt = _res._render_targets.get(_default_rt_handle);
  _immediate_context->OMSetRenderTargets(1, &(rt->rtv.p), rt->dsv);
  _immediate_context->RSSetViewports(1, &_viewport);
}

bool Graphics::close()
{
  delete this;
  _instance = NULL;
  return true;
}

void Graphics::clear(GraphicsObjectHandle h, const XMFLOAT4 &c) {
  RenderTargetData *rt = _res._render_targets.get(h);
  _immediate_context->ClearRenderTargetView(rt->rtv, &c.x);
  _immediate_context->ClearDepthStencilView(rt->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

void Graphics::clear(const XMFLOAT4& c)
{
  RenderTargetData *rt = _res._render_targets.get(_default_rt_handle);
  _immediate_context->ClearRenderTargetView(rt->rtv, &c.x);
  _immediate_context->ClearDepthStencilView(rt->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

void Graphics::present()
{
  const DWORD now = timeGetTime();
  if (_start_fps_time == 0xffffffff) {
    _start_fps_time = now;
  } else if (++_frame_count == 50) {
    _fps = 50.0f * 1000.0f / (timeGetTime() - _start_fps_time);
    _start_fps_time = now;
    _frame_count = 0;
  }

  _swap_chain->Present(0,0);
}

void Graphics::resize(const int width, const int height)
{
  if (!_swap_chain || width == 0 || height == 0)
    return;
  create_back_buffers(width, height);
}

GraphicsObjectHandle Graphics::create_constant_buffer(int size) {
  CD3D11_BUFFER_DESC desc(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);

  const int idx = _res._constant_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_buffer_inner(_device, desc, NULL, &_res._constant_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kConstantBuffer, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_static_vertex_buffer(uint32_t data_size, const void* data) {
  const int idx = _res._vertex_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_static_vertex_buffer(data_size, 1, data, &_res._vertex_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kVertexBuffer, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_static_index_buffer(uint32_t data_size, const void* data) {
  const int idx = _res._index_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_static_index_buffer(data_size, 1, data, &_res._index_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kIndexBuffer, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_input_layout(const D3D11_INPUT_ELEMENT_DESC *desc, int elems, void *shader_bytecode, int len) {
  const int idx = _res._input_layouts.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateInputLayout(desc, elems, shader_bytecode, len, &_res._input_layouts[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kInputLayout, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_vertex_shader(void *shader_bytecode, int len, const string &id) {

  int idx = _res._vertex_shaders.find_by_token(id);
  if (idx != -1 || (idx = _res._vertex_shaders.find_free_index()) != -1) {
    ID3D11VertexShader *vs = nullptr;
    if (SUCCEEDED(_device->CreateVertexShader(shader_bytecode, len, NULL, &vs))) {
      SAFE_RELEASE(_res._vertex_shaders[idx].first);
      _res._vertex_shaders[idx].first = vs;
      return GraphicsObjectHandle(GraphicsObjectHandle::kVertexShader, 0, idx);
    }
  }
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_pixel_shader(void *shader_bytecode, int len, const string &id) {

  int idx = _res._pixel_shaders.find_by_token(id);
  if (idx != -1 || (idx = _res._pixel_shaders.find_free_index()) != -1) {
    ID3D11PixelShader *ps = nullptr;
    if (SUCCEEDED(_device->CreatePixelShader(shader_bytecode, len, NULL, &ps))) {
      SAFE_RELEASE(_res._pixel_shaders[idx].first);
      _res._pixel_shaders[idx].first = ps;
      return GraphicsObjectHandle(GraphicsObjectHandle::kPixelShader, 0, idx);
    }
  }
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_rasterizer_state(const D3D11_RASTERIZER_DESC &desc) {
  const int idx = _res._rasterizer_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateRasterizerState(&desc, &_res._rasterizer_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kRasterizerState, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_blend_state(const D3D11_BLEND_DESC &desc) {
  const int idx = _res._blend_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateBlendState(&desc, &_res._blend_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kBlendState, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC &desc) {
  const int idx = _res._depth_stencil_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateDepthStencilState(&desc, &_res._depth_stencil_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kDepthStencilState, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_sampler_state(const D3D11_SAMPLER_DESC &desc) {
  const int idx = _res._sampler_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateSamplerState(&desc, &_res._sampler_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kSamplerState, 0, idx);
  return GraphicsObjectHandle();
}
#if 0
static bool technique_changed_callback(const char *filename) {
	return true;
}

static bool create_techniques_from_file(ResourceInterface *ri, const char *filename, vector<Technique *> *techniques, vector<Material *> *materials) {
  void *buf;
  size_t len;
  if (!ri->load_file(filename, &buf, &len))
    return false;

  // TODO
  if (!ri->supports_file_watch()) {
    //load_inner(filename);
  } else {
		ri->add_file_watch(filename, true, technique_changed_callback);
  }


  techniques->clear();
  TechniqueParser parser;
  vector<Technique *> tmp;
  bool res = parser.parse(&GRAPHICS, (const char *)buf, (const char *)buf + len, &tmp, materials);
  if (res) {
    for (size_t i = 0; i < tmp.size(); ++i) {
      if (tmp[i]->init(&GRAPHICS, ri)) {
        techniques->push_back(tmp[i]);
      } else {
        delete exch_null(tmp[i]);
        res = false;
        break;
      }
    }
  }
  return res;
}
#endif
void Graphics::technique_file_changed(const char *filename, void *token) {

}

void Graphics::shader_file_changed(const char *filename, void *token) {
  Technique *t = (Technique *)token;
}

bool Graphics::load_techniques(const char *filename, bool add_materials) {
  bool res = true;
  vector<Material *> materials;

  vector<Technique *> loaded_techniques;
  vector<uint8> buf;
  if (!_ri->load_file(filename, &buf))
    return false;

  TechniqueParser parser;
  vector<Technique *> tmp;
  if (!parser.parse(&GRAPHICS, (const char *)&buf[0], (const char *)&buf[buf.size()-1] + 1, &tmp, &materials))
    return false;

  if (add_materials) {
    for (auto it = begin(materials); it != end(materials); ++it)
      MATERIAL_MANAGER.add_material(*it, true);
  }

  auto fails = stable_partition(begin(tmp), end(tmp), [&](Technique *t) { return t->init(&GRAPHICS, _ri); });
  // delete all the techniques that fail to initialize
  if (fails != end(tmp)) {
    for (auto i = fails; i != end(tmp); ++i) {
      delete exch_null(*i);
    }
    tmp.erase(fails, end(tmp));
  }

  auto &v = _techniques_by_file[filename];
  v.clear();

  for (auto i = begin(tmp); i != end(tmp); ++i) {
    Technique *t = *i;

    if (_ri->supports_file_watch()) {
      if (Shader *vs = t->vertex_shader()) {
        _ri->add_file_watch(vs->source_filename().c_str(), false, t, bind(&Graphics::shader_file_changed, this, _1, _2));
      }
      if (Shader *ps = t->pixel_shader()) {
        _ri->add_file_watch(ps->source_filename().c_str(), false, t, bind(&Graphics::shader_file_changed, this, _1, _2));
      }
    }

    int idx = _res._techniques.find_by_token(t->name());
    if (idx != -1 || (idx = _res._techniques.find_free_index()) != -1) {
      SAFE_DELETE(_res._techniques[idx].first);
      _res._techniques[idx] = make_pair(t, t->name());
      v.push_back(t->name());
    } else {
      SAFE_DELETE(t);
    }
  }

  if (_ri->supports_file_watch()) {
    _ri->add_file_watch(filename, false, NULL, bind(&Graphics::technique_file_changed, this, _1, _2));
  }

  return res;
}

GraphicsObjectHandle Graphics::find_technique(const char *name) {
  int idx = _res._techniques.find_by_token(name);
  return idx != -1 ? GraphicsObjectHandle(GraphicsObjectHandle::kTechnique, 0, idx) : GraphicsObjectHandle();
}


void Graphics::set_samplers(Technique *technique, Shader *shader) {

  ID3D11DeviceContext *ctx = _immediate_context;

  const int num_samplers = (int)shader->sampler_params().size();
  if (!num_samplers)
    return;

  vector<ID3D11SamplerState *> samplers(num_samplers);
  for (int i = 0; i < num_samplers; ++i) {
    const SamplerParam &s = shader->sampler_params()[i];
    samplers[s.bind_point] = _res._sampler_states.get(technique->sampler_state(s.name.c_str()));
  }

  ctx->PSSetSamplers(0, num_samplers, &samplers[0]);
}

void Graphics::unbind_resource_views(int resource_bitmask) {

  ID3D11ShaderResourceView *null_view = nullptr;

  ID3D11DeviceContext *ctx = _immediate_context;
  for (int i = 0; i < 8; ++i) {
    if (resource_bitmask & (1 << i))
      ctx->PSSetShaderResources(i, 1, &null_view);
  }
}

void Graphics::set_resource_views(Technique *technique, Shader *shader, int *resources_set) {

  ID3D11DeviceContext *ctx = _immediate_context;

  const int num_views = (int)shader->resource_view_params().size();
  if (!num_views)
    return;

  vector<ID3D11ShaderResourceView *> views(num_views);
  for (int i = 0; i < num_views; ++i) {
    const ResourceViewParam &v = shader->resource_view_params()[i];
    // look for the named texture, and if that doesn't exist, look for a render target
    int idx = _res._textures.find_by_token(v.name);
    if (idx != -1) {
      views[v.bind_point] = _res._textures[idx].first->srv;
    } else {
      idx = _res._render_targets.find_by_token(v.name);
      if (idx != -1) {
        views[v.bind_point] = _res._render_targets[idx].first->srv;
      } else {
        LOG_WARNING_LN("Texture not found: %s", v.name.c_str());
      }
    }
  }

  ctx->PSSetShaderResources(0, num_views, &views[0]);

  *resources_set = 0;
  for (int i = 0; i < num_views; ++i) 
    *resources_set |= (1 << i) * (views[i] != nullptr);

}

void Graphics::set_cbuffer_params(Technique *technique, Shader *shader, uint16 material_id, intptr_t mesh_id) {

  ID3D11DeviceContext *ctx = _immediate_context;

  vector<CBuffer> &cbuffers = technique->get_cbuffers();

  // copy the parameters into the technique's cbuffer staging area
  for (size_t i = 0; i < shader->cbuffer_params().size(); ++i) {
    const CBufferParam &p = shader->cbuffer_params()[i];
    const CBuffer &cb = cbuffers[p.cbuffer];
    switch (p.source) {
      case PropertySource::kMaterial:
        memcpy((void *)&cb.staging[p.start_offset], 
               &PROPERTY_MANAGER.get_material_property<XMFLOAT4>(material_id, p.name.c_str()), p.size);
        break;
      case PropertySource::kMesh: {
          XMFLOAT4X4 mtx = PROPERTY_MANAGER.get_mesh_property<XMFLOAT4X4>(mesh_id, p.name.c_str());
          memcpy((void *)&cb.staging[p.start_offset], &mtx, p.size);
          break;
        }
      case PropertySource::kSystem: {
          XMFLOAT4X4 mtx = PROPERTY_MANAGER.get_system_property<XMFLOAT4X4>(p.name.c_str());
          memcpy((void *)&cb.staging[p.start_offset], &mtx, p.size);
          break;
        }
      case PropertySource::kUser:
        break;
    }
  }

  // commit the staging area
  for (auto i = begin(cbuffers); i != end(cbuffers); ++i) {
    const CBuffer &cb = *i;
    ID3D11Buffer *buffer = _res._constant_buffers.get(cb.handle);
    ctx->UpdateSubresource(buffer, 0, NULL, &cb.staging[0], 0, 0);
    if (shader->type() == Shader::kVertexShader)
      ctx->VSSetConstantBuffers(0, 1, &buffer);
    else
      ctx->PSSetConstantBuffers(0, 1, &buffer);
  }
}

bool Graphics::map(GraphicsObjectHandle h, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *res) {
  return SUCCEEDED(_immediate_context->Map(_res._textures.get(h)->texture, sub, type, flags, res));
}

void Graphics::unmap(GraphicsObjectHandle h, UINT sub) {
  _immediate_context->Unmap(_res._textures.get(h)->texture, sub);
}

void Graphics::copy_resource(GraphicsObjectHandle dst, GraphicsObjectHandle src) {
  _immediate_context->CopyResource(_res._textures.get(dst)->texture, _res._textures.get(src)->texture);
}

