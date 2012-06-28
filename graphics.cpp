#include "stdafx.h"
#include "graphics.hpp"
#include "logger.hpp"
#include "technique.hpp"
#include "technique_parser.hpp"
#include "resource_interface.hpp"
#include "property_manager.hpp"
#include "material_manager.hpp"
#include "string_utils.hpp"
#include "tracked_location.hpp"
#include "mesh.hpp"

using namespace std;
using namespace std::tr1::placeholders;

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "DXGUID.lib")
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "D3DX11.lib")

namespace
{
  template <class T>
  void set_private_data(const TrackedLocation &loc, T *t) {
#if WITH_TRACKED_LOCATION
    t->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(loc.function), loc.function);
#else
#endif
  }

  HRESULT create_buffer_inner(const TrackedLocation &loc, ID3D11Device* device, const D3D11_BUFFER_DESC& desc, const void* data, ID3D11Buffer** buffer)
  {
    D3D11_SUBRESOURCE_DATA init_data;
    ZeroMemory(&init_data, sizeof(init_data));
    init_data.pSysMem = data;
    HRESULT hr = device->CreateBuffer(&desc, data ? &init_data : NULL, buffer);
    set_private_data(loc, *buffer);
    return hr;
  }

  uint32 multiple_of_16(uint32 a) {
    return (a + 15) & ~0xf;
  }

  static const int cEffectDataSize = 1024 * 1024;
}

Graphics* Graphics::_instance = NULL;

Graphics::Graphics()
  : _width(-1)
  , _height(-1)
  , _start_fps_time(0xffffffff)
  , _frame_count(0)
  , _fps(0)
  , _vs_profile("vs_4_0")
  , _ps_profile("ps_4_0")
  , _screen_size_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("System::g_screen_size"))
  , _effect_data_ofs(0)
  , _vertex_shaders(release_obj<ID3D11VertexShader *>)
  , _pixel_shaders(release_obj<ID3D11PixelShader *>)
  , _vertex_buffers(release_obj<ID3D11Buffer *>)
  , _index_buffers(release_obj<ID3D11Buffer *>)
  , _constant_buffers(release_obj<ID3D11Buffer *>)
  , _techniques(delete_obj<Technique *>)
  , _input_layouts(release_obj<ID3D11InputLayout *>)
  , _blend_states(release_obj<ID3D11BlendState *>)
  , _depth_stencil_states(release_obj<ID3D11DepthStencilState *>)
  , _rasterizer_states(release_obj<ID3D11RasterizerState *>)
  , _sampler_states(release_obj<ID3D11SamplerState *>)
  , _shader_resource_views(release_obj<ID3D11ShaderResourceView *>)
  , _textures(delete_obj<TextureResource *>)
  , _render_targets(delete_obj<RenderTargetResource *>)
  , _resources(delete_obj<SimpleResource *>)
#if WITH_GWEN
  , _font_wrappers(release_obj<IFW1FontWrapper *>)
#endif

{
  _effect_data.resize(cEffectDataSize);
}

Graphics::~Graphics()
{
}

bool Graphics::create() {
  assert(!_instance);
  _instance = new Graphics();
  return true;
}


HRESULT Graphics::create_static_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data, ID3D11Buffer** vertex_buffer) 
{
  return create_buffer_inner(loc, _device, 
    CD3D11_BUFFER_DESC(multiple_of_16(buffer_size), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE), data, vertex_buffer);
}

HRESULT Graphics::create_static_index_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data, ID3D11Buffer** index_buffer) {
  return create_buffer_inner(loc, _device, 
    CD3D11_BUFFER_DESC(multiple_of_16(buffer_size), D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_IMMUTABLE), data, index_buffer);
}

HRESULT Graphics::create_dynamic_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, ID3D11Buffer** vertex_buffer)
{
  return create_buffer_inner(loc, _device, 
    CD3D11_BUFFER_DESC(multiple_of_16(buffer_size), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), 
    NULL, vertex_buffer);
}

void Graphics::set_vb(ID3D11Buffer *buf, uint32_t stride)
{
  UINT ofs[] = { 0 };
  ID3D11Buffer* bufs[] = { buf };
  uint32_t strides[] = { stride };
  _immediate_context->IASetVertexBuffers(0, 1, bufs, strides, ofs);
}

GraphicsObjectHandle Graphics::create_render_target(
  const TrackedLocation &loc, int width, int height, bool shader_resource, bool depth_buffer, DXGI_FORMAT format, const char *name) {

    RenderTargetResource *data = new RenderTargetResource;
    if (!create_render_target(loc, width, height, shader_resource, depth_buffer, format, data)) {
      delete exch_null(data);
      return GraphicsObjectHandle();
    }

    int idx = name ? _render_targets.idx_from_token(name) : -1;
    if (idx != -1 || (idx = _render_targets.find_free_index()) != -1) {
      if (_render_targets[idx])
        _render_targets[idx]->reset();
      _render_targets.set_pair(idx, make_pair(name, data));
      return GraphicsObjectHandle(GraphicsObjectHandle::kRenderTarget, 0, idx);
    }
    return GraphicsObjectHandle();

}

GraphicsObjectHandle Graphics::create_render_target(const TrackedLocation &loc, int width, int height, bool shader_resource, const char *name) {
  return create_render_target(loc, width, height, true, true, DXGI_FORMAT_R32G32B32A32_FLOAT, name);
}

bool Graphics::create_render_target(
  const TrackedLocation &loc, int width, int height, bool shader_resource, bool depth_buffer, DXGI_FORMAT format, 
  RenderTargetResource *out)
{
  out->reset();

  // create the render target
  out->texture.desc = CD3D11_TEXTURE2D_DESC(format, width, height, 1, 1, 
    D3D11_BIND_RENDER_TARGET | shader_resource * D3D11_BIND_SHADER_RESOURCE);
  B_WRN_HR(_device->CreateTexture2D(&out->texture.desc, NULL, &out->texture.resource.p));
  set_private_data(loc, out->texture.resource.p);

  // create the render target view
  out->rtv.desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, out->texture.desc.Format);
  B_WRN_HR(_device->CreateRenderTargetView(out->texture.resource, &out->rtv.desc, &out->rtv.resource.p));
  set_private_data(loc, out->rtv.resource.p);
  float color[4] = { 0 };
  _immediate_context->ClearRenderTargetView(out->rtv.resource.p, color);

  if (depth_buffer) {
    // create the depth stencil texture
    out->depth_stencil.desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
    B_WRN_HR(_device->CreateTexture2D(&out->depth_stencil.desc, NULL, &out->depth_stencil.resource.p));
    set_private_data(loc, out->depth_stencil.resource.p);

    // create depth stencil view
    out->dsv.desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT);
    B_WRN_HR(_device->CreateDepthStencilView(out->depth_stencil.resource, &out->dsv.desc, &out->dsv.resource.p));
    set_private_data(loc, out->dsv.resource.p);
  }

  if (shader_resource) {
    // create the shader resource view
    out->srv.desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture.desc.Format);
    B_WRN_HR(_device->CreateShaderResourceView(out->texture.resource, &out->srv.desc, &out->srv.resource.p));
    set_private_data(loc, out->srv.resource.p);
  }

  return true;
}

bool Graphics::read_texture(const char *filename, D3DX11_IMAGE_INFO *info, uint32 *pitch, vector<uint8> *bits) {

  HRESULT hr;
  D3DX11GetImageInfoFromFile(filename, NULL, info, &hr);
  if (FAILED(hr))
    return false;

  D3DX11_IMAGE_LOAD_INFO loadinfo;
  ZeroMemory(&loadinfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
  loadinfo.CpuAccessFlags = D3D11_CPU_ACCESS_READ;
  loadinfo.Usage = D3D11_USAGE_STAGING;
  loadinfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  CComPtr<ID3D11Resource> resource;
  D3DX11CreateTextureFromFile(GRAPHICS.device(), filename, &loadinfo, NULL, &resource.p, &hr);
  if (FAILED(hr))
    return false;

  D3D11_MAPPED_SUBRESOURCE sub;
  if (FAILED(GRAPHICS.context()->Map(resource, 0, D3D11_MAP_READ, 0, &sub)))
    return false;

  uint8 *src = (uint8 *)sub.pData;
  bits->resize(sub.RowPitch * info->Height);
  uint8 *dst = &(*bits)[0];

  int row_size = 4 * info->Width;
  for (uint32 i = 0; i < info->Height; ++i) {
    memcpy(&dst[i*sub.RowPitch], &src[i*sub.RowPitch], row_size);
  }

  GRAPHICS.context()->Unmap(resource, 0);
  *pitch = sub.RowPitch;
  return true;
}

GraphicsObjectHandle Graphics::get_texture(const char *filename) {
  int idx = _resources.idx_from_token(filename);
  return make_goh(GraphicsObjectHandle::kResource, idx);
}

GraphicsObjectHandle Graphics::load_texture(const char *filename, const char *friendly_name, D3DX11_IMAGE_INFO *info) {

  if (info) {
    HRESULT hr;
    D3DX11GetImageInfoFromFile(filename, NULL, info, &hr);
    if (FAILED(hr))
      return GraphicsObjectHandle();
  }

  auto *data = new SimpleResource();
  HRESULT hr;
  D3DX11CreateTextureFromFile(GRAPHICS.device(), filename, NULL, NULL, &data->resource, &hr);
  if (FAILED(hr)) {
    return GraphicsObjectHandle();
  }

  auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
  if (FAILED(_device->CreateShaderResourceView(data->resource, &desc, &data->view.resource)))
    return GraphicsObjectHandle();

  int idx = _resources.idx_from_token(friendly_name);
  if (idx != -1 || (idx = _resources.find_free_index()) != -1) {
    if (_resources[idx])
      _resources[idx]->reset();
    _resources.set_pair(idx, make_pair(friendly_name, data));
  }
  return make_goh(GraphicsObjectHandle::kResource, idx);
}

GraphicsObjectHandle Graphics::insert_texture(TextureResource *data, const char *friendly_name) {

  int idx = friendly_name ? _textures.idx_from_token(friendly_name) : -1;
  if (idx != -1 || (idx = _textures.find_free_index()) != -1) {
    if (_textures[idx])
      _textures[idx]->reset();
    _textures.set_pair(idx, make_pair(friendly_name, data));
  }
  return make_goh(GraphicsObjectHandle::kTexture, idx);
}

GraphicsObjectHandle Graphics::create_texture(const TrackedLocation &loc, const D3D11_TEXTURE2D_DESC &desc, const char *name) {
  TextureResource *data = new TextureResource;
  if (!create_texture(loc, desc, data)) {
    delete exch_null(data);
    return GraphicsObjectHandle();
  }
  return insert_texture(data, name);
}

bool Graphics::create_texture(const TrackedLocation &loc, const D3D11_TEXTURE2D_DESC &desc, TextureResource *out)
{
  out->reset();

  // create the texture
  out->texture.desc = desc;
  B_WRN_HR(_device->CreateTexture2D(&out->texture.desc, NULL, &out->texture.resource.p));
  set_private_data(loc, out->texture.resource.p);

  // create the shader resource view if the texture has a shader resource bind flag
  if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
    out->view.desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture.desc.Format);
    B_WRN_HR(_device->CreateShaderResourceView(out->texture.resource, &out->view.desc, &out->view.resource.p));
    set_private_data(loc, out->view.resource.p);
  }

  return true;
}

GraphicsObjectHandle Graphics::create_texture(const TrackedLocation &loc, int width, int height, DXGI_FORMAT fmt, 
                                              void *data_bits, int data_width, int data_height, int data_pitch, const char *friendly_name) {
  TextureResource *data = new TextureResource;
  if (!create_texture(loc, width, height, fmt, data_bits, data_width, data_height, data_pitch, data)) {
    delete exch_null(data);
    return GraphicsObjectHandle();
  }
  return insert_texture(data, friendly_name);
}

bool Graphics::create_texture(const TrackedLocation &loc, int width, int height, DXGI_FORMAT fmt, 
                              void *data, int data_width, int data_height, int data_pitch, 
                              TextureResource *out)
{
  if (!create_texture(loc, CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), out))
    return false;

  D3D11_MAPPED_SUBRESOURCE resource;
  B_ERR_HR(_immediate_context->Map(out->texture.resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
  uint8_t *src = (uint8_t *)data;
  uint8_t *dst = (uint8_t *)resource.pData;
  const int w = std::min<int>(width, data_width);
  const int h = std::min<int>(height, data_height);
  for (int i = 0; i < h; ++i) {
    memcpy(dst, src, w);
    src += data_pitch;
    dst += resource.RowPitch;
  }
  _immediate_context->Unmap(out->texture.resource, 0);
  return true;
}

bool Graphics::create_back_buffers(int width, int height)
{
  _width = width;
  _height = height;

  int idx = _render_targets.idx_from_token("default_rt");
  RenderTargetResource *rt = nullptr;
  if (idx != -1) {
    rt = _render_targets.get(idx);
    rt->reset();
    // release any existing buffers
    //_immediate_context->ClearState();
    _swap_chain->ResizeBuffers(1, width, height, _buffer_format, 0);
  } else {
    rt = new RenderTargetResource;
    idx = _render_targets.find_free_index();
  }

  // Get the dx11 back buffer
  B_ERR_HR(_swap_chain->GetBuffer(0, IID_PPV_ARGS(&rt->texture.resource)));
  rt->texture.resource->GetDesc(&rt->texture.desc);

  B_ERR_HR(_device->CreateRenderTargetView(rt->texture.resource, NULL, &rt->rtv.resource));
  set_private_data(FROM_HERE, rt->rtv.resource.p);
  rt->rtv.resource->GetDesc(&rt->rtv.desc);

  // depth buffer
  B_ERR_HR(_device->CreateTexture2D(
    &CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL), 
    NULL, &rt->depth_stencil.resource));
  rt->depth_stencil.resource->GetDesc(&rt->depth_stencil.desc);
  set_private_data(FROM_HERE, rt->depth_stencil.resource.p);

  B_ERR_HR(_device->CreateDepthStencilView(rt->depth_stencil.resource, NULL, &rt->dsv.resource));
  set_private_data(FROM_HERE, rt->dsv.resource.p);
  rt->dsv.resource->GetDesc(&rt->dsv.desc);

  _render_targets.set_pair(idx, make_pair("default_rt", rt));
  _default_render_target = GraphicsObjectHandle(GraphicsObjectHandle::kRenderTarget, 0, idx);

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
  IDXGIFactory1 *dxgi_factory = nullptr;
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
    exch_null(adapter)->Release();
    if (wcscmp(desc.Description, L"NVIDIA PerfHud") == 0) {
      perfhud = i;
    }
  }
  exch_null(dxgi_factory)->Release();

  B_ERR_BOOL(!adapters.empty());

  adapter = perfhud != -1 ? adapters[perfhud] : adapters.front();

  int flags = 0;
#ifdef _DEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  // Create the DX11 device
  B_ERR_HR(D3D11CreateDeviceAndSwapChain(
    adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &sd, &_swap_chain.p, &_device.p,
    &_feature_level, &_immediate_context.p));
  set_private_data(FROM_HERE, _immediate_context.p);

  B_ERR_BOOL(_feature_level >= D3D_FEATURE_LEVEL_9_3);

#ifdef _DEBUG
  B_ERR_HR(_device->QueryInterface(IID_ID3D11Debug, (void **)&(_d3d_debug.p)));
#endif

  B_ERR_BOOL(create_back_buffers(width, height));

  B_ERR_HR(_device->CreateDepthStencilState(&CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT()), &_default_depth_stencil_state.p));
  B_ERR_HR(_device->CreateRasterizerState(&CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT()), &_default_rasterizer_state.p));
  B_ERR_HR(_device->CreateBlendState(&CD3D11_BLEND_DESC(CD3D11_DEFAULT()), &_default_blend_state.p));

  set_private_data(FROM_HERE, _default_depth_stencil_state.p);
  set_private_data(FROM_HERE, _default_rasterizer_state.p);
  set_private_data(FROM_HERE, _default_blend_state.p);

  _device->CreateClassLinkage(&_class_linkage.p);

  // Create a dummy texture
  DWORD black = 0;
  _dummy_texture = create_texture(FROM_HERE, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &black, 1, 1, 1, "dummy_texture");

  for (int i = 0; i < 4; ++i)
    _default_blend_factors[i] = 1.0f;
  return true;
}

void Graphics::set_default_render_target()
{
  RenderTargetResource *rt = _render_targets.get(_default_render_target);
  _immediate_context->OMSetRenderTargets(1, &rt->rtv.resource.p, rt->dsv.resource);
  _immediate_context->RSSetViewports(1, &_viewport);
}

bool Graphics::close() {
  assert(_instance);
  delete exch_null(_instance);
  return true;
}

void Graphics::clear(GraphicsObjectHandle h, const XMFLOAT4 &c) {
  RenderTargetResource *rt = _render_targets.get(h);
  _immediate_context->ClearRenderTargetView(rt->rtv.resource, &c.x);
  _immediate_context->ClearDepthStencilView(rt->dsv.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
}

void Graphics::clear(const XMFLOAT4& c)
{
  RenderTargetResource *rt = _render_targets.get(_default_render_target);
  _immediate_context->ClearRenderTargetView(rt->rtv.resource, &c.x);
  _immediate_context->ClearDepthStencilView(rt->dsv.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
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
  PROPERTY_MANAGER.set_property(_screen_size_id, XMFLOAT4((float)width, (float)height, 0, 0));
  create_back_buffers(width, height);
}

GraphicsObjectHandle Graphics::create_constant_buffer(const TrackedLocation &loc, int buffer_size, bool dynamic) {
  CD3D11_BUFFER_DESC desc(multiple_of_16(buffer_size), D3D11_BIND_CONSTANT_BUFFER, 
    dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT, 
    dynamic ? D3D11_CPU_ACCESS_WRITE : 0);

  const int idx = _constant_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_buffer_inner(loc, _device, desc, NULL, &_constant_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kConstantBuffer, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_static_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data) {
  const int idx = _vertex_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_static_vertex_buffer(loc, buffer_size, data, &_vertex_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kVertexBuffer, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_dynamic_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size) {
  const int idx = _vertex_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_dynamic_vertex_buffer(loc, buffer_size, &_vertex_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kVertexBuffer, 0, idx);
  return GraphicsObjectHandle();
}


GraphicsObjectHandle Graphics::create_static_index_buffer(const TrackedLocation &loc, uint32_t data_size, const void* data) {
  const int idx = _index_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_static_index_buffer(loc, data_size, data, &_index_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kIndexBuffer, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_input_layout(const TrackedLocation &loc, const D3D11_INPUT_ELEMENT_DESC *desc, int elems, const void *shader_bytecode, int len) {
  const int idx = _input_layouts.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateInputLayout(desc, elems, shader_bytecode, len, &_input_layouts[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kInputLayout, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_vertex_shader(const TrackedLocation &loc, void *shader_bytecode, int len, const string &id) {

  int idx = _vertex_shaders.idx_from_token(id);
  if (idx != -1 || (idx = _vertex_shaders.find_free_index()) != -1) {
    ID3D11VertexShader *vs = nullptr;
    if (SUCCEEDED(_device->CreateVertexShader(shader_bytecode, len, NULL, &vs))) {
      set_private_data(loc, vs);
      SAFE_RELEASE(_vertex_shaders[idx]);
      _vertex_shaders.set_pair(idx, make_pair(id, vs));
      return GraphicsObjectHandle(GraphicsObjectHandle::kVertexShader, 0, idx);
    }
  }
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_pixel_shader(const TrackedLocation &loc, void *shader_bytecode, int len, const string &id) {

  int idx = _pixel_shaders.idx_from_token(id);
  if (idx != -1 || (idx = _pixel_shaders.find_free_index()) != -1) {
    ID3D11PixelShader *ps = nullptr;
    if (SUCCEEDED(_device->CreatePixelShader(shader_bytecode, len, NULL, &ps))) {
      set_private_data(loc, ps);
      SAFE_RELEASE(_pixel_shaders[idx]);
      _pixel_shaders.set_pair(idx, make_pair(id, ps));
      return GraphicsObjectHandle(GraphicsObjectHandle::kPixelShader, 0, idx);
    }
  }
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_rasterizer_state(const TrackedLocation &loc, const D3D11_RASTERIZER_DESC &desc) {
  const int idx = _rasterizer_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateRasterizerState(&desc, &_rasterizer_states[idx]))) {
    return GraphicsObjectHandle(GraphicsObjectHandle::kRasterizerState, 0, idx);
  }
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_blend_state(const TrackedLocation &loc, const D3D11_BLEND_DESC &desc) {
  const int idx = _blend_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateBlendState(&desc, &_blend_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kBlendState, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_depth_stencil_state(const TrackedLocation &loc, const D3D11_DEPTH_STENCIL_DESC &desc) {
  const int idx = _depth_stencil_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateDepthStencilState(&desc, &_depth_stencil_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kDepthStencilState, 0, idx);
  return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_sampler_state(const TrackedLocation &loc, const D3D11_SAMPLER_DESC &desc) {
  const int idx = _sampler_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateSamplerState(&desc, &_sampler_states[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kSamplerState, 0, idx);
  return GraphicsObjectHandle();
}

#if WITH_GWEN
GraphicsObjectHandle Graphics::get_or_create_font_family(const wstring &name) {
  int idx = _font_wrappers.idx_from_token(name);
  if (idx != -1)
    return GraphicsObjectHandle(GraphicsObjectHandle::kFontFamily, 0, idx);
  if (!_fw1_factory)
    FW1CreateFactory(FW1_VERSION, &_fw1_factory.p);
  idx = _font_wrappers.find_free_index();
  if (idx != -1) {
    IFW1FontWrapper *font = nullptr;
    _fw1_factory->CreateFontWrapper(_device, name.c_str(), &font);
    _font_wrappers.set_pair(idx, make_pair(name, font));
    return GraphicsObjectHandle(GraphicsObjectHandle::kFontFamily, 0, idx);
  }
  return GraphicsObjectHandle();
}

bool Graphics::measure_text(GraphicsObjectHandle font, const std::wstring &family, const std::wstring &text, float size, uint32 flags, FW1_RECTF *rect) {
  if (IFW1FontWrapper *wrapper = _font_wrappers.get(font)) {
    FW1_RECTF layout_rect;
    layout_rect.Left = layout_rect.Right = layout_rect.Top = layout_rect.Bottom = 0;
    *rect = wrapper->MeasureString(text.c_str(), family.c_str(), size, &layout_rect, flags | FW1_NOWORDWRAP);
    return true;
  }
  return false;
}
#endif

bool Graphics::technique_file_changed(const char *filename, void *token) {
  return true;
}

bool Graphics::shader_file_changed(const char *filename, void *token) {
  // TODO
/*
  LOG_CONTEXT("%s loading: %s", __FUNCTION__, filename);

  Technique *t = (Technique *)token;
  for (int i = 0; i < t->vertex_shader_count(); ++i) {
    if (Shader *shader = t->vertex_shader(i)) {
      if (!t->create_shaders(_ri, shader)) {
        LOG_WARNING_LN("Error initializing vertex shader: %s", filename);
        return false;
      }
    }
  }

  for (int i = 0; i < t->pixel_shader_count(); ++i) {
    if (Shader *shader = t->pixel_shader(i)) {
      if (!t->init_shader(_ri, shader)) {
        LOG_WARNING_LN("Error initializing pixel shader: %s", filename);
        return false;
      }
    }
  }
*/
  return true;
}

bool Graphics::load_techniques(const char *filename, bool add_materials) {

  LOG_CONTEXT("%s loading: %s", __FUNCTION__, filename);

  bool res = true;
  vector<Material *> materials;

  vector<Technique *> loaded_techniques;
  vector<char> buf;
  B_ERR_BOOL(RESOURCE_MANAGER.load_file(filename, &buf));
  //LOG_VERBOSE_LN("loaded: %s, %d", filename, buf.size());

  TechniqueParser parser;
  vector<Technique *> tmp;
  B_ERR_BOOL(parser.parse((const char *)&buf[0], (const char *)&buf[buf.size()-1] + 1, &tmp, &materials));

  if (add_materials) {
    for (auto it = begin(materials); it != end(materials); ++it)
      MATERIAL_MANAGER.add_material(*it, true);
  }

  // Init the techniques
  for (auto it = begin(tmp); it != end(tmp); ) {
    Technique *t = *it;
    if (!t->init()) {
      LOG_WARNING_LN("init failed for technique: %s. Error msg: %s", t->name().c_str(), t->error_msg().c_str());
      delete exch_null(t);
      it = tmp.erase(it);
    } else {
      ++it;
    }
  }

  auto &v = _techniques_by_file[filename];
  v.clear();

  for (auto i = begin(tmp); i != end(tmp); ++i) {
    Technique *t = *i;

    if (RESOURCE_MANAGER.supports_file_watch()) {
      auto shader_changed = bind(&Graphics::shader_file_changed, this, _1, _2);
      if (Shader *vs = t->vertex_shader(0)) {
        RESOURCE_MANAGER.add_file_watch(vs->source_filename().c_str(), t, shader_changed, false, nullptr, -1);
      }
      if (Shader *ps = t->pixel_shader(0)) {
        RESOURCE_MANAGER.add_file_watch(ps->source_filename().c_str(), t, shader_changed, false, nullptr, -1);
      }
    }

    int idx = _techniques.idx_from_token(t->name());
    if (idx != -1 || (idx = _techniques.find_free_index()) != -1) {
      SAFE_DELETE(_techniques[idx]);
      _techniques.set_pair(idx, make_pair(t->name(), t));
      v.push_back(t->name());
    } else {
      SAFE_DELETE(t);
    }
  }

  if (RESOURCE_MANAGER.supports_file_watch()) {
    RESOURCE_MANAGER.add_file_watch(filename, NULL, bind(&Graphics::technique_file_changed, this, _1, _2), false, nullptr, -1);
  }

  return res;
}

GraphicsObjectHandle Graphics::find_resource(const std::string &name) {

  if (name.empty())
    return _dummy_texture;

  // check textures, then resources, then render targets
  int idx = _textures.idx_from_token(name);
  if (idx != -1)
    return GraphicsObjectHandle(GraphicsObjectHandle::kTexture, 0, idx);
  idx = _resources.idx_from_token(name);
  if (idx != -1)
    return GraphicsObjectHandle(GraphicsObjectHandle::kResource, 0, idx);
  idx = _render_targets.idx_from_token(name);
  return make_goh(GraphicsObjectHandle::kRenderTarget, idx);
}

GraphicsObjectHandle Graphics::find_technique(const char *name) {
  int idx = _techniques.idx_from_token(name);
  assert(idx != -1);
  return idx != -1 ? GraphicsObjectHandle(GraphicsObjectHandle::kTechnique, 0, idx) : GraphicsObjectHandle();
}

void Graphics::get_technique_states(const char *name, GraphicsObjectHandle *rs, GraphicsObjectHandle *bs, GraphicsObjectHandle *dss) {
  if (Technique *technique = _techniques.find(name, (Technique *)NULL)) {
    if (rs) *rs = technique->rasterizer_state();
    if (bs) *bs = technique->blend_state();
    if (dss) *dss = technique->depth_stencil_state();
  }
}

GraphicsObjectHandle Graphics::find_sampler_state(const std::string &name) {
  int idx = _sampler_states.idx_from_token(name);
  return make_goh(GraphicsObjectHandle::kSamplerState, idx);
}

GraphicsObjectHandle Graphics::find_input_layout(const std::string &technique_name) {
  if (Technique *technique = _techniques.find(technique_name, (Technique *)NULL))
    return technique->input_layout();
  return GraphicsObjectHandle();
}

void Graphics::unbind_resource_views(int resource_bitmask) {

  ID3D11ShaderResourceView *null_view = nullptr;

  ID3D11DeviceContext *ctx = _immediate_context;
  for (int i = 0; i < 8; ++i) {
    if (resource_bitmask & (1 << i))
      ctx->PSSetShaderResources(i, 1, &null_view);
  }
}

bool Graphics::map(GraphicsObjectHandle h, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *res) {
  switch (h._type) {
    case GraphicsObjectHandle::kTexture:
      return SUCCEEDED(_immediate_context->Map(_textures.get(h)->texture.resource, sub, type, flags, res));

    case GraphicsObjectHandle::kVertexBuffer:
      return SUCCEEDED(_immediate_context->Map(_vertex_buffers.get(h), sub, type, flags, res));

    case GraphicsObjectHandle::kIndexBuffer:
      return SUCCEEDED(_immediate_context->Map(_index_buffers.get(h), sub, type, flags, res));

    default:
      LOG_WARNING_LN("Invalid resource type passed to %s", __FUNCTION__);
      return false;
  }
}

void Graphics::unmap(GraphicsObjectHandle h, UINT sub) {
  switch (h._type) {
    case GraphicsObjectHandle::kTexture:
      _immediate_context->Unmap(_textures.get(h)->texture.resource, sub);
      break;

    case GraphicsObjectHandle::kVertexBuffer:
      _immediate_context->Unmap(_vertex_buffers.get(h), sub);
      break;

    case GraphicsObjectHandle::kIndexBuffer:
      _immediate_context->Unmap(_index_buffers.get(h), sub);
      break;

    default:
      LOG_WARNING_LN("Invalid resource type passed to %s", __FUNCTION__);
  }
}

void Graphics::copy_resource(GraphicsObjectHandle dst, GraphicsObjectHandle src) {
  _immediate_context->CopyResource(_textures.get(dst)->texture.resource, _textures.get(src)->texture.resource);
}

Technique *Graphics::get_technique(GraphicsObjectHandle h) {
  return _techniques.get(h);
}

ID3D11ClassLinkage *Graphics::get_class_linkage() {
  return _class_linkage.p;
}

void Graphics::add_shader_flag(const std::string &flag) {
  auto it = _shader_flags.find(flag);
  if (it == _shader_flags.end()) {
    _shader_flags[flag] = 1 << _shader_flags.size();
  }
}

int Graphics::get_shader_flag(const std::string &flag) {
  auto it = _shader_flags.find(flag);
  return it == _shader_flags.end() ? 0 : it->second;
}

GraphicsObjectHandle Graphics::make_goh(GraphicsObjectHandle::Type type, int idx) {
  return idx != -1 ? GraphicsObjectHandle(type, 0, idx) : GraphicsObjectHandle();
}

void Graphics::render() {

  ID3D11DeviceContext *ctx = GRAPHICS.context();

  ctx->RSSetViewports(1, &GRAPHICS.viewport());

  // sort by keys (just using the depth)
  //stable_sort(begin(_render_commands), end(_render_commands), 
//    [&](const RenderCmd &a, const RenderCmd &b) { return (a.key.data & 0xffff000000000000) < (b.key.data & 0xffff000000000000); });

  //RenderObjects objects;
  Technique *prev_technique = nullptr;

  for (auto it = begin(_render_commands), e = end(_render_commands); it != e; ++it) {
    const RenderCmd &cmd = *it;
    RenderKey key = cmd.key;
    void *data = cmd.data;

    switch (key.cmd) {

      case RenderKey::kSetRenderTarget: {
        ID3D11RenderTargetView *rts[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        ID3D11DepthStencilView *dsv = nullptr;
        int num_targets = 0;
        if (!data) {
          // set default render target
          Graphics::RenderTargetResource *rt = _render_targets.get(GRAPHICS.default_render_target());
          rts[0] = rt->rtv.resource;
          dsv = rt->dsv.resource;
          num_targets = 1;

        } else {
          RenderTargetData *rt_data = (RenderTargetData *)data;
          int i;
          float color[4] = {0,0,0,0};
          // Collect the valid render targets, set the first available depth buffer
          // and clear targets if specified
          for (i = 0; i < 8; ++i) {
            GraphicsObjectHandle h = rt_data->render_targets[i];
            if (!h.is_valid())
              break;
            Graphics::RenderTargetResource *rt = _render_targets.get(h);
            if (!dsv && rt->dsv.resource) {
              dsv = rt->dsv.resource;
            }
            rts[i] = rt->rtv.resource;
            // clear render target (and depth stenci)
            if (rt_data->clear_target[i]) {
              ctx->ClearRenderTargetView(rts[i], color);
              if (rt->dsv.resource) {
                ctx->ClearDepthStencilView(rt->dsv.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
              }
            }
          }
          num_targets = i;
        }
        ctx->OMSetRenderTargets(num_targets, rts, dsv);
        break;
      }

      case RenderKey::kRenderMesh: {
        const MeshRenderData *render_data = (MeshRenderData *)data;
        const MeshGeometry *geometry = render_data->geometry;
        Technique *technique = _techniques.get(render_data->technique);
        const Material *material = MATERIAL_MANAGER.get_material(render_data->material);
        const Mesh *mesh = render_data->mesh;

        // get the shader for the current technique, based on the flags used by the material
        int flags = material->flags();
        Shader *vs = technique->vertex_shader(flags);
        Shader *ps = technique->pixel_shader(flags);

        set_vs(vs->handle());
        set_ps(ps->handle());

        set_vb(geometry->vb, geometry->vertex_size);
        set_ib(geometry->ib, geometry->index_format);
        set_topology(geometry->topology);

        set_layout(technique->input_layout());
        set_rs(technique->rasterizer_state());
        set_dss(technique->depth_stencil_state(), default_stencil_ref());
        set_bs(technique->blend_state(), default_blend_factors(), default_sample_mask());

        // set cbuffers
        auto &vs_cbuffers = vs->get_cbuffers();
        for (size_t i = 0; i < vs_cbuffers.size(); ++i) {
          mesh->fill_cbuffer(&vs_cbuffers[i]);
          material->fill_cbuffer(&vs_cbuffers[i]);
          technique->fill_cbuffer(&vs_cbuffers[i]);
        }

        auto &ps_cbuffers = ps->get_cbuffers();
        for (size_t i = 0; i < ps_cbuffers.size(); ++i) {
          mesh->fill_cbuffer(&ps_cbuffers[i]);
          material->fill_cbuffer(&ps_cbuffers[i]);
          technique->fill_cbuffer(&ps_cbuffers[i]);
        }
        set_cbuffer(vs_cbuffers, ps_cbuffers);

        // set samplers
        auto &samplers = ps->samplers();
        if (samplers.count > 0) {
          vector<GraphicsObjectHandle> ss;
          technique->fill_samplers(samplers, &ss);
          set_samplers(&ss[0], samplers.first, samplers.count);
        }

        // set resource views
        auto &rv = ps->resource_views();
        if (rv.count > 0) {
          vector<GraphicsObjectHandle> rr;
          material->fill_resource_views(rv, &rr);
          technique->fill_resource_views(rv, &rr);
          set_shader_resources(&rr[0], rv.first, rv.count);
        }

        draw_indexed(geometry->index_count, 0, 0);

        if (rv.count > 0)
          unset_shader_resource(rv.first, rv.count);
        break;
      }

      case RenderKey::kRenderTechnique: {
        Technique *technique = _techniques.get(key.handle);
        const TechniqueRenderData *render_data = &technique->render_data();

        Shader *vs = technique->vertex_shader(0);
        Shader *ps = technique->pixel_shader(0);
        set_vs(vs->handle());
        set_ps(ps->handle());

        set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        set_vb(technique->vb(), technique->vertex_size());
        set_layout(technique->input_layout());
        set_ib(technique->ib(), technique->index_format());

        set_rs(technique->rasterizer_state());
        set_dss(technique->depth_stencil_state(), default_stencil_ref());
        set_bs(technique->blend_state(), default_blend_factors(), default_sample_mask());

        // set cbuffers
        auto &vs_cbuffers = vs->get_cbuffers();
        for (size_t i = 0; i < vs_cbuffers.size(); ++i) {
          technique->fill_cbuffer(&vs_cbuffers[i]);
        }

        auto &ps_cbuffers = ps->get_cbuffers();
        for (size_t i = 0; i < ps_cbuffers.size(); ++i) {
          technique->fill_cbuffer(&ps_cbuffers[i]);
        }
        set_cbuffer(vs_cbuffers, ps_cbuffers);

        // set samplers
        auto &samplers = ps->samplers();
        if (samplers.count > 0) {
          vector<GraphicsObjectHandle> ss;
          technique->fill_samplers(samplers, &ss);
          set_samplers(&ss[0], samplers.first, samplers.count);
        }

        // set resource views
        auto &rv = ps->resource_views();
        if (rv.count > 0) {
          vector<GraphicsObjectHandle> rr;
          technique->fill_resource_views(rv, &rr);
          set_shader_resources(&rr[0], rv.first, rv.count);
        }

        draw_indexed(technique->index_count(), 0, 0);

        if (rv.count > 0)
          unset_shader_resource(rv.first, rv.count);
        break;
      }
    }
  }

  _render_commands.clear();
  _effect_data_ofs = 0;
}

void *Graphics::raw_alloc(size_t size) {
  if (size + _effect_data_ofs >= cEffectDataSize)
    return nullptr;

  void *ptr = &_effect_data[_effect_data_ofs];
  assert(_effect_data_ofs + size <= _effect_data.size());
  _effect_data_ofs += size;
  return ptr;
}

void Graphics::validate_command(RenderKey key, const void *data) {

  switch (key.cmd) {

    case RenderKey::kSetRenderTarget:
      break;

    case RenderKey::kRenderMesh: {
      // TODO
/*
      MeshRenderData *render_data = (MeshRenderData *)data;
      Technique *technique = _techniques.get(render_data->cur_technique);
      assert(technique);
      Shader *vertex_shader = technique->vertex_shader(0);
      assert(vertex_shader);
      Shader *pixel_shader = technique->pixel_shader(0);
      assert(pixel_shader);
*/
      break;
    }

    case RenderKey::kRenderTechnique:
      break;
  }
}

void Graphics::submit_command(const TrackedLocation &location, RenderKey key, void *data) {
#if _DEBUG
  validate_command(key, data);
#endif
  _render_commands.push_back(RenderCmd(location, key, data));
}

void Graphics::submit_technique(GraphicsObjectHandle technique) {
  RenderKey key;
  key.cmd = RenderKey::kRenderTechnique;
  key.handle = technique;
  submit_command(FROM_HERE, key, nullptr);
}

void Graphics::set_vs(GraphicsObjectHandle vs) {
  if (prev_vs != vs) {
    _immediate_context->VSSetShader(_vertex_shaders.get(vs), NULL, 0);
    prev_vs = vs;
  }
}

void Graphics::set_ps(GraphicsObjectHandle ps) {
  if (prev_ps != ps) {
    _immediate_context->PSSetShader(_pixel_shaders.get(ps), NULL, 0);
    prev_ps = ps;
  }
}

void Graphics::set_layout(GraphicsObjectHandle layout) {
  if (prev_layout != layout) {
    _immediate_context->IASetInputLayout(_input_layouts.get(layout));
    prev_layout = layout;
  }
}

void Graphics::set_vb(GraphicsObjectHandle vb, int vertex_size) {
  if (prev_vb != vb) {
    set_vb(_vertex_buffers.get(vb), vertex_size);
    prev_vb = vb;
  }
}

void Graphics::set_ib(GraphicsObjectHandle ib, DXGI_FORMAT format) {
  if (prev_ib != ib) {
    _immediate_context->IASetIndexBuffer(_index_buffers.get(ib), format, 0);
    prev_ib = ib;
  }
}

void Graphics::set_topology(D3D11_PRIMITIVE_TOPOLOGY top) {
  if (prev_topology != top) {
    _immediate_context->IASetPrimitiveTopology(top);
    prev_topology = top;
  }
}

void Graphics::set_rs(GraphicsObjectHandle rs) {
  if (prev_rs != rs) {
    _immediate_context->RSSetState(_rasterizer_states.get(rs));
    prev_rs = rs;
  }
}

void Graphics::set_dss(GraphicsObjectHandle dss, UINT stencil_ref) {
  if (prev_dss != dss) {
    _immediate_context->OMSetDepthStencilState(_depth_stencil_states.get(dss), stencil_ref);
    prev_dss = dss;
  }
}

void Graphics::set_bs(GraphicsObjectHandle bs, const float *blend_factors, UINT sample_mask) {
  if (prev_bs != bs) {
    _immediate_context->OMSetBlendState(_blend_states.get(bs), blend_factors, sample_mask);
    prev_bs = bs;
  }
}

void Graphics::set_samplers(const GraphicsObjectHandle *sampler_handles, int first_sampler, int num_samplers) {
  if (!num_samplers)
    return;
  if (memcmp(sampler_handles, prev_samplers, num_samplers * sizeof(GraphicsObjectHandle))) {
    ID3D11SamplerState *samplers[MAX_SAMPLERS];
    for (int i = 0; i < num_samplers; ++i) {
      samplers[i] = _sampler_states.get(sampler_handles[i]);
    }

    _immediate_context->PSSetSamplers(first_sampler, num_samplers, samplers);
    memcpy(prev_samplers, sampler_handles, num_samplers * sizeof(GraphicsObjectHandle));
  }
}

void Graphics::set_shader_resources(const GraphicsObjectHandle *view_handles, int first_view, int num_views) {
  if (!num_views)
    return;
  // force setting the views because we always unset them..
  bool force = true;
  if (force || memcmp(view_handles, prev_views, num_views * sizeof(GraphicsObjectHandle))) {
    ID3D11ShaderResourceView *views[MAX_SAMPLERS];
    bool valid_views = true;
    for (int i = 0; i < num_views; ++i) {
      GraphicsObjectHandle h = view_handles[i+first_view];
      if (h.type() == GraphicsObjectHandle::kTexture) {
        auto *data = _textures.get(h);
        views[i] = data->view.resource;
      } else if (h.type() == GraphicsObjectHandle::kResource) {
        auto *data = _resources.get(h);
        views[i] = data->view.resource;
      } else if (h.type() == GraphicsObjectHandle::kRenderTarget) {
        auto *data = _render_targets.get(h);
        views[i] = data->srv.resource;
      } else {
        valid_views = false;
      }
    }
    if (valid_views) {
      _immediate_context->PSSetShaderResources(first_view, num_views, views);
      memcpy(prev_views, view_handles+first_view, num_views * sizeof(GraphicsObjectHandle));
    }
  }
}

void Graphics::unset_shader_resource(int first_view, int num_views) {
  if (!num_views)
    return;
  static ID3D11ShaderResourceView *null_views[MAX_SAMPLERS] = {0, 0, 0, 0, 0, 0, 0, 0};
  _immediate_context->PSSetShaderResources(first_view, num_views, null_views);
}

void Graphics::set_cbuffer(const vector<CBuffer> &vs, const vector<CBuffer> &ps) {

  ID3D11Buffer **vs_cb = (ID3D11Buffer **)_alloca(vs.size() * sizeof(ID3D11Buffer *));
  ID3D11Buffer **ps_cb = (ID3D11Buffer **)_alloca(ps.size() * sizeof(ID3D11Buffer *));

  // Copy the vs cbuffers
  for (size_t i = 0; i < vs.size(); ++i) {
    auto &cur = vs[i];
    ID3D11Buffer *buffer = _constant_buffers.get(cur.handle);
    D3D11_MAPPED_SUBRESOURCE sub;
    _immediate_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, cur.staging.data(), cur.staging.size());
    _immediate_context->Unmap(buffer, 0);
    vs_cb[i] = buffer;
  }

  // Copy the ps cbuffers
  for (size_t i = 0; i < ps.size(); ++i) {
    auto &cur = ps[i];
    ID3D11Buffer *buffer = _constant_buffers.get(cur.handle);
    D3D11_MAPPED_SUBRESOURCE sub;
    _immediate_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, cur.staging.data(), cur.staging.size());
    _immediate_context->Unmap(buffer, 0);
    ps_cb[i] = buffer;
  }

  if (!vs.empty())
    _immediate_context->VSSetConstantBuffers(0, vs.size(), vs_cb);

  if (!ps.empty())
    _immediate_context->PSSetConstantBuffers(0, ps.size(), ps_cb);

}

void Graphics::draw_indexed(int count, int start_index, int base_vertex) {
  _immediate_context->DrawIndexed(count, start_index, base_vertex);
}
