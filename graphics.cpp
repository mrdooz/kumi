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
#include "vertex_types.hpp"
#include "deferred_context.hpp"
#include "profiler.hpp"
#include "effect.hpp"

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

static GraphicsObjectHandle emptyGoh;

Graphics* Graphics::_instance;

Graphics::Graphics()
  : _width(-1)
  , _height(-1)
  , _start_fps_time(0xffffffff)
  , _frame_count(0)
  , _fps(0)
  , _vs_profile("vs_5_0")
  , _ps_profile("ps_5_0")
  , _cs_profile("cs_5_0")
  , _gs_profile("gs_5_0")
  , _screen_size_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("System::g_screen_size"))
  , _vertex_shaders(release_obj<ID3D11VertexShader *>)
  , _pixel_shaders(release_obj<ID3D11PixelShader *>)
  , _compute_shaders(release_obj<ID3D11ComputeShader *>)
  , _geometry_shaders(release_obj<ID3D11GeometryShader *>)
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
  , _structured_buffers(delete_obj<StructuredBuffer *>)
  , _vsync(false)
{
}

bool Graphics::create() {
  KASSERT(!_instance);
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

GraphicsObjectHandle Graphics::get_temp_render_target(const TrackedLocation &loc, 
    int width, int height, DXGI_FORMAT format, uint32 bufferFlags, const std::string &name) {

  KASSERT(_render_targets._key_to_idx.find(name) == _render_targets._key_to_idx.end());

  // look for a free render target with the wanted properties
  UINT flags = (bufferFlags & kCreateMipMaps) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

  auto cmp_desc = [&](const D3D11_TEXTURE2D_DESC &desc) {
    return desc.Width == width && desc.Height == height && desc.Format == format && desc.MiscFlags == flags;
  };

  for (int idx = 0; idx < _render_targets.Size; ++idx) {
    if (auto rt = _render_targets[idx]) {
      auto &desc = rt->texture.desc;
      if (!rt->in_use && cmp_desc(desc) && !!(bufferFlags & kCreateDepthBuffer) == !!rt->depth_stencil.resource.p) {
        rt->in_use = true;
        _render_targets._key_to_idx[name] = idx;
        _render_targets._idx_to_key[idx] = name;
        auto goh = make_goh(GraphicsObjectHandle::kRenderTarget, idx);
        auto pid = PROPERTY_MANAGER.get_or_create<GraphicsObjectHandle>(name);
        PROPERTY_MANAGER.set_property(pid, goh);
        return goh;
      }
    }
  }
  // nothing suitable found, so we create a render target
  return create_render_target(loc, width, height, format, bufferFlags, name);
}

void Graphics::release_temp_render_target(GraphicsObjectHandle h) {
  auto rt = _render_targets.get(h);
  KASSERT(rt->in_use);
  rt->in_use = false;
  int idx = h.id();
  string key = _render_targets._idx_to_key[idx];
  _render_targets._idx_to_key.erase(idx);
  _render_targets._key_to_idx.erase(key);
}

GraphicsObjectHandle Graphics::create_structured_buffer(const TrackedLocation &loc, int elemSize, int numElems, bool createSrv) {

  unique_ptr<StructuredBuffer> sb(new StructuredBuffer);

  // Create Structured Buffer
  D3D11_BUFFER_DESC sbDesc;
  sbDesc.BindFlags            = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
  sbDesc.CPUAccessFlags       = 0;
  sbDesc.MiscFlags            = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  sbDesc.StructureByteStride  = elemSize;
  sbDesc.ByteWidth            = elemSize * numElems;
  sbDesc.Usage                = D3D11_USAGE_DEFAULT;
  if (FAILED(_device->CreateBuffer(&sbDesc, NULL, &sb->buffer.resource)))
    return emptyGoh;

  auto buf = sb->buffer.resource.p;
  set_private_data(loc, sb->buffer.resource.p);

  // create the UAV for the structured buffer
  D3D11_UNORDERED_ACCESS_VIEW_DESC sbUAVDesc;
  sbUAVDesc.Buffer.FirstElement       = 0;
  sbUAVDesc.Buffer.Flags              = 0;
  sbUAVDesc.Buffer.NumElements        = numElems;
  sbUAVDesc.Format                    = DXGI_FORMAT_UNKNOWN;
  sbUAVDesc.ViewDimension             = D3D11_UAV_DIMENSION_BUFFER;
  if (FAILED(_device->CreateUnorderedAccessView(buf, &sbUAVDesc, &sb->uav.resource)))
    return emptyGoh;

  set_private_data(loc, sb->uav.resource.p);

  if (createSrv) {
    // create the Shader Resource View (SRV) for the structured buffer
    D3D11_SHADER_RESOURCE_VIEW_DESC sbSRVDesc;
    sbSRVDesc.Buffer.ElementOffset          = 0;
    sbSRVDesc.Buffer.ElementWidth           = elemSize;
    sbSRVDesc.Buffer.FirstElement           = 0;
    sbSRVDesc.Buffer.NumElements            = numElems;
    sbSRVDesc.Format                        = DXGI_FORMAT_UNKNOWN;
    sbSRVDesc.ViewDimension                 = D3D11_SRV_DIMENSION_BUFFER;
    if (FAILED(_device->CreateShaderResourceView(buf, &sbSRVDesc, &sb->srv.resource)))
      return emptyGoh;

    set_private_data(loc, sb->srv.resource.p);

  }

  int idx = _structured_buffers.find_free_index();
  if (idx != -1) {
    _structured_buffers[idx] = sb.release();
    return make_goh(GraphicsObjectHandle::kStructuredBuffer, idx);
  }

  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_render_target(const TrackedLocation &loc, int width, int height, DXGI_FORMAT format, uint32 bufferFlags, const std::string &name) {

    unique_ptr<RenderTargetResource> data(new RenderTargetResource);
    if (create_render_target(loc, width, height, format, bufferFlags, data.get())) {
      int idx = !name.empty() ? _render_targets.idx_from_token(name) : -1;
      if (idx != -1 || (idx = _render_targets.find_free_index()) != -1) {
        if (_render_targets[idx])
          _render_targets[idx]->reset();
        _render_targets.set_pair(idx, make_pair(name, data.release()));
        auto goh = make_goh(GraphicsObjectHandle::kRenderTarget, idx);
        auto pid = PROPERTY_MANAGER.get_or_create<GraphicsObjectHandle>(name);
        PROPERTY_MANAGER.set_property(pid, goh);
        return goh;
      }
    }
    return emptyGoh;
}

bool Graphics::create_render_target(const TrackedLocation &loc, int width, int height, DXGI_FORMAT format, uint32 bufferFlags, RenderTargetResource *out)
{
  out->reset();

  // create the render target
  int mip_levels = (bufferFlags & kCreateMipMaps) ? 0 : 1;
  uint32 flags = D3D11_BIND_RENDER_TARGET 
    | (bufferFlags & kCreateSrv ? D3D11_BIND_SHADER_RESOURCE : 0)
    | (bufferFlags & kCreateUav ? D3D11_BIND_UNORDERED_ACCESS : 0);

  out->texture.desc = CD3D11_TEXTURE2D_DESC(format, width, height, 1, mip_levels, flags);
  out->texture.desc.MiscFlags = (bufferFlags & kCreateMipMaps) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
  B_ERR_HR(_device->CreateTexture2D(&out->texture.desc, NULL, &out->texture.resource.p));
  set_private_data(loc, out->texture.resource.p);

  // create the render target view
  out->rtv.desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, out->texture.desc.Format);
  B_ERR_HR(_device->CreateRenderTargetView(out->texture.resource, &out->rtv.desc, &out->rtv.resource.p));
  set_private_data(loc, out->rtv.resource.p);
  float color[4] = { 0 };
  _immediate_context->ClearRenderTargetView(out->rtv.resource.p, color);

  if (bufferFlags & kCreateDepthBuffer) {
    // create the depth stencil texture
    out->depth_stencil.desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
    B_ERR_HR(_device->CreateTexture2D(&out->depth_stencil.desc, NULL, &out->depth_stencil.resource.p));
    set_private_data(loc, out->depth_stencil.resource.p);

    // create depth stencil view
    out->dsv.desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT);
    B_ERR_HR(_device->CreateDepthStencilView(out->depth_stencil.resource, &out->dsv.desc, &out->dsv.resource.p));
    set_private_data(loc, out->dsv.resource.p);
  }

  if (bufferFlags & kCreateSrv) {
    // create the shader resource view
    out->srv.desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture.desc.Format);
    B_ERR_HR(_device->CreateShaderResourceView(out->texture.resource, &out->srv.desc, &out->srv.resource.p));
    set_private_data(loc, out->srv.resource.p);
  }


  if (bufferFlags & kCreateUav) {
    out->uav.desc = CD3D11_UNORDERED_ACCESS_VIEW_DESC(D3D11_UAV_DIMENSION_TEXTURE2D, format, 0, 0, width*height);
    B_ERR_HR(_device->CreateUnorderedAccessView(out->texture.resource, &out->uav.desc, &out->uav.resource));
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
  D3DX11CreateTextureFromFile(_device, filename, &loadinfo, NULL, &resource.p, &hr);
  if (FAILED(hr))
    return false;

  D3D11_MAPPED_SUBRESOURCE sub;
  if (FAILED(_immediate_context->Map(resource, 0, D3D11_MAP_READ, 0, &sub)))
    return false;

  uint8 *src = (uint8 *)sub.pData;
  bits->resize(sub.RowPitch * info->Height);
  uint8 *dst = &(*bits)[0];

  int row_size = 4 * info->Width;
  for (uint32 i = 0; i < info->Height; ++i) {
    memcpy(&dst[i*sub.RowPitch], &src[i*sub.RowPitch], row_size);
  }

  _immediate_context->Unmap(resource, 0);
  *pitch = sub.RowPitch;
  return true;
}

GraphicsObjectHandle Graphics::get_texture(const char *filename) {
  int idx = _resources.idx_from_token(filename);
  return make_goh(GraphicsObjectHandle::kResource, idx);
}

GraphicsObjectHandle Graphics::load_texture(const char *filename, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info) {

  if (info && FAILED(D3DX11GetImageInfoFromFile(filename, NULL, info, NULL)))
    return emptyGoh;

  auto data = unique_ptr<SimpleResource>(new SimpleResource());
  if (FAILED(D3DX11CreateTextureFromFile(_device, filename, NULL, NULL, &data->resource, NULL)))
    return emptyGoh;

  // TODO: allow for srgb loading
  auto fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
  auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, fmt);
  if (FAILED(_device->CreateShaderResourceView(data->resource, &desc, &data->view.resource)))
    return emptyGoh;

  int idx = _resources.idx_from_token(friendly_name);
  if (idx != -1 || (idx = _resources.find_free_index()) != -1) {
    if (_resources[idx])
      _resources[idx]->reset();
    _resources.set_pair(idx, make_pair(friendly_name, data.release()));
  }
  return make_goh(GraphicsObjectHandle::kResource, idx);
}

GraphicsObjectHandle Graphics::load_texture_from_memory(const void *buf, size_t len, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info) {

  if (info && FAILED(D3DX11GetImageInfoFromMemory(buf, len, NULL, info, NULL)))
    return emptyGoh;

  auto data = unique_ptr<SimpleResource>(new SimpleResource());
  if (FAILED(D3DX11CreateTextureFromMemory(_device, buf, len, NULL, NULL, &data->resource, NULL)))
    return emptyGoh;

  // TODO: allow for srgb loading
  auto fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
  auto desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, fmt);
  if (FAILED(_device->CreateShaderResourceView(data->resource, &desc, &data->view.resource)))
    return emptyGoh;

  int idx = _resources.idx_from_token(friendly_name);
  if (idx != -1 || (idx = _resources.find_free_index()) != -1) {
    if (_resources[idx])
      _resources[idx]->reset();
    _resources.set_pair(idx, make_pair(friendly_name, data.release()));
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
    return emptyGoh;
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
    return emptyGoh;
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

bool Graphics::create_default_geometry() {

  static const uint16 quadIndices[] = {
    0, 1, 2,
    2, 1, 3
  };

  {
    // fullscreen pos/tex quad
    static const float verts[] = {
      -1, +1, +1, +0, +0,
      +1, +1, +1, +1, +0,
      -1, -1, +1, +0, +1,
      +1, -1, +1, +1, +1
    };


    auto vb = create_static_vertex_buffer(FROM_HERE, sizeof(verts), verts);
    auto ib = create_static_index_buffer(FROM_HERE, sizeof(quadIndices), quadIndices);
    _predefined_geometry.insert(make_pair(kGeomFsQuadPosTex, make_pair(vb, ib)));
  }

  // fullscreen pos quad
  {
    static const float verts[] = {
      -1, +1, +1,
      +1, +1, +1,
      -1, -1, +1,
      +1, -1, +1,
    };

    auto vb = create_static_vertex_buffer(FROM_HERE, sizeof(verts), verts);
    auto ib = create_static_index_buffer(FROM_HERE, sizeof(quadIndices), quadIndices);
    _predefined_geometry.insert(make_pair(kGeomFsQuadPos, make_pair(vb, ib)));
  }

  return true;
}

bool Graphics::init(const HWND hwnd, const int width, const int height)
{
  _width = width;
  _height = height;
  _buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM; //DXGI_FORMAT_R8G8B8A8_UNORM;

  DXGI_SWAP_CHAIN_DESC swapchain_desc;
  ZeroMemory(&swapchain_desc, sizeof( swapchain_desc ) );
  swapchain_desc.BufferCount = 3;
  swapchain_desc.BufferDesc.Width = width;
  swapchain_desc.BufferDesc.Height = height;
  swapchain_desc.BufferDesc.Format = _buffer_format;
  swapchain_desc.BufferDesc.RefreshRate.Numerator = 0;
  swapchain_desc.BufferDesc.RefreshRate.Denominator = 1;
  swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapchain_desc.OutputWindow = hwnd;
  swapchain_desc.SampleDesc.Count = 1;
  swapchain_desc.SampleDesc.Quality = 0;
  swapchain_desc.Windowed = TRUE;

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
    adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, 
    D3D11_SDK_VERSION, &swapchain_desc, &_swap_chain.p, &_device.p,
    &_feature_level, &_immediate_context.p));
  set_private_data(FROM_HERE, _immediate_context.p);

  B_ERR_BOOL(_feature_level >= D3D_FEATURE_LEVEL_9_3);

#ifdef _DEBUG
  B_ERR_HR(_device->QueryInterface(IID_ID3D11Debug, (void **)&(_d3d_debug.p)));
#endif

  B_ERR_BOOL(create_back_buffers(width, height));

  _default_depth_stencil_state = create_depth_stencil_state(FROM_HERE, CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT()));
  _default_rasterizer_state = create_rasterizer_state(FROM_HERE, CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT()));
  _default_blend_state = create_blend_state(FROM_HERE, CD3D11_BLEND_DESC(CD3D11_DEFAULT()));

  _device->CreateClassLinkage(&_class_linkage.p);

  create_default_geometry();

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
  _swap_chain->Present(_vsync ? 1 : 0,0);
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
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_static_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data) {
  const int idx = _vertex_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_static_vertex_buffer(loc, buffer_size, data, &_vertex_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kVertexBuffer, 0, idx);
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_dynamic_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size) {
  const int idx = _vertex_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_dynamic_vertex_buffer(loc, buffer_size, &_vertex_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kVertexBuffer, 0, idx);
  return emptyGoh;
}


GraphicsObjectHandle Graphics::create_static_index_buffer(const TrackedLocation &loc, uint32_t data_size, const void* data) {
  const int idx = _index_buffers.find_free_index();
  if (idx != -1 && SUCCEEDED(create_static_index_buffer(loc, data_size, data, &_index_buffers[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kIndexBuffer, 0, idx);
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_input_layout(const TrackedLocation &loc, const std::vector<D3D11_INPUT_ELEMENT_DESC> &desc, const std::vector<char> &shader_bytecode) {
  const int idx = _input_layouts.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateInputLayout(&desc[0], desc.size(), &shader_bytecode[0], shader_bytecode.size(), &_input_layouts[idx])))
    return GraphicsObjectHandle(GraphicsObjectHandle::kInputLayout, 0, idx);
  return emptyGoh;
}

template<typename T, class Cont>
int add_shader(const TrackedLocation &loc, int idx, Cont &cont, T *shader, const string &id, GraphicsObjectHandle::Type type) {
  set_private_data(loc, shader);
  SAFE_RELEASE(cont[idx]);
  cont.set_pair(idx, make_pair(id, shader));
  return Graphics::make_goh(type, idx);
}

GraphicsObjectHandle Graphics::create_vertex_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const string &id) {

  int idx = _vertex_shaders.idx_from_token(id);
  if (idx != -1 || (idx = _vertex_shaders.find_free_index()) != -1) {
    ID3D11VertexShader *vs = nullptr;
    if (SUCCEEDED(_device->CreateVertexShader(&shader_bytecode[0], shader_bytecode.size(), NULL, &vs))) {
      return add_shader(loc, idx, _vertex_shaders, vs, id, GraphicsObjectHandle::kVertexShader);
    }
  }
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_pixel_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const string &id) {

  int idx = _pixel_shaders.idx_from_token(id);
  if (idx != -1 || (idx = _pixel_shaders.find_free_index()) != -1) {
    ID3D11PixelShader *ps = nullptr;
    if (SUCCEEDED(_device->CreatePixelShader(&shader_bytecode[0], shader_bytecode.size(), NULL, &ps))) {
      return add_shader(loc, idx, _pixel_shaders, ps, id, GraphicsObjectHandle::kPixelShader);
    }
  }
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_compute_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const string &id) {

  int idx = _compute_shaders.idx_from_token(id);
  if (idx != -1 || (idx = _compute_shaders.find_free_index()) != -1) {
    ID3D11ComputeShader *cs = nullptr;
    if (SUCCEEDED(_device->CreateComputeShader(&shader_bytecode[0], shader_bytecode.size(), NULL, &cs))) {
      return add_shader(loc, idx, _compute_shaders, cs, id, GraphicsObjectHandle::kComputeShader);
    }
  }
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_geometry_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const string &id) {

  int idx = _geometry_shaders.idx_from_token(id);
  if (idx != -1 || (idx = _geometry_shaders.find_free_index()) != -1) {
    ID3D11GeometryShader *cs = nullptr;
    if (SUCCEEDED(_device->CreateGeometryShader(&shader_bytecode[0], shader_bytecode.size(), NULL, &cs))) {
      return add_shader(loc, idx, _geometry_shaders, cs, id, GraphicsObjectHandle::kGeometryShader);
    }
  }
  return emptyGoh;
}

template<typename T, class Cont>
int find_existing(const T &desc, Cont &c) {
  // look for an existing state
  T d2;
  for (int i = 0; i < c.Size; ++i) {
    if (!c[i])
      continue;
    c[i]->GetDesc(&d2);
    if (0 == memcmp(&d2, &desc, sizeof(T)))
      return i;
  }
  return -1;
}

GraphicsObjectHandle Graphics::create_rasterizer_state(const TrackedLocation &loc, const D3D11_RASTERIZER_DESC &desc) {
  int idx = find_existing(desc, _rasterizer_states);
  if (idx != -1)
    return make_goh(GraphicsObjectHandle::kRasterizerState, idx);

  idx = _rasterizer_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateRasterizerState(&desc, &_rasterizer_states[idx]))) {
    return make_goh(GraphicsObjectHandle::kRasterizerState, idx);
  }
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_blend_state(const TrackedLocation &loc, const D3D11_BLEND_DESC &desc) {
  int idx = find_existing(desc, _blend_states);
  if (idx != -1)
    return make_goh(GraphicsObjectHandle::kBlendState, idx);

  idx = _blend_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateBlendState(&desc, &_blend_states[idx])))
    return make_goh(GraphicsObjectHandle::kBlendState, idx);
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_depth_stencil_state(const TrackedLocation &loc, const D3D11_DEPTH_STENCIL_DESC &desc) {
  int idx = find_existing(desc, _depth_stencil_states);
  if (idx != -1)
    return make_goh(GraphicsObjectHandle::kDepthStencilState, idx);

  idx = _depth_stencil_states.find_free_index();
  if (idx != -1 && SUCCEEDED(_device->CreateDepthStencilState(&desc, &_depth_stencil_states[idx])))
    return make_goh(GraphicsObjectHandle::kDepthStencilState, idx);
  return emptyGoh;
}

GraphicsObjectHandle Graphics::create_sampler(const TrackedLocation &loc, const std::string &name, const D3D11_SAMPLER_DESC &desc) {
  // TODO: skip finding duplicates for now, until we've sorted out the whole naming issue..
/*
  int idx = find_existing(desc, _sampler_states);
  if (idx != -1)
    return make_goh(GraphicsObjectHandle::kSamplerState, idx);
*/
  int idx = _sampler_states.find_free_index();
  ID3D11SamplerState *ss;
  if (idx != -1 && SUCCEEDED(_device->CreateSamplerState(&desc, &ss))) {
    _sampler_states.set_pair(idx, make_pair(name, ss));
    return make_goh(GraphicsObjectHandle::kSamplerState, idx);
  }
  return emptyGoh;
}

bool Graphics::technique_file_changed(const char *filename, void *token) {
  return true;
}

bool Graphics::shader_file_changed(const char *filename, void *token) {
  // TODO
/*
  LOG_CONTEXT("%st loading: %s", __FUNCTION__, filename);

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

  TechniqueFile result;
  TechniqueParser parser(filename, &result);
  B_ERR_BOOL(parser.parse());

  vector<Material *> &materials = result.materials;
  vector<Technique *> &tmp = result.techniques;

  if (add_materials) {
    for (auto it = begin(materials); it != end(materials); ++it)
      MATERIAL_MANAGER.add_material(*it, true);
  }

  // Init the techniques
  for (auto it = begin(tmp); it != end(tmp); ) {
    Technique *t = *it;
    if (!t->init()) {
      LOG_ERROR_LN("init failed for technique: %s (%s). Error msg: %s", t->name().c_str(), filename, t->error_msg().c_str());
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

    auto shader_changed = bind(&Graphics::shader_file_changed, this, _1, _2);
    if (Shader *vs = t->vertex_shader(0)) {
#if WITH_UNPACKED_RESOUCES
      RESOURCE_MANAGER.add_file_watch(vs->source_filename().c_str(), t, shader_changed, false, nullptr, -1);
#endif
    }
    if (Shader *ps = t->pixel_shader(0)) {
#if WITH_UNPACKED_RESOUCES
      RESOURCE_MANAGER.add_file_watch(ps->source_filename().c_str(), t, shader_changed, false, nullptr, -1);
#endif
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

#if WITH_UNPACKED_RESOUCES
  RESOURCE_MANAGER.add_file_watch(filename, NULL, bind(&Graphics::technique_file_changed, this, _1, _2), false, nullptr, -1);
#endif

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
  KASSERT(idx != -1);
  return idx != -1 ? GraphicsObjectHandle(GraphicsObjectHandle::kTechnique, 0, idx) : emptyGoh;
}

GraphicsObjectHandle Graphics::find_sampler(const std::string &name) {
  int idx = _sampler_states.idx_from_token(name);
  return make_goh(GraphicsObjectHandle::kSamplerState, idx);
}

/*
GraphicsObjectHandle Graphics::find_input_layout(const std::string &technique_name) {
  if (Technique *technique = _techniques.find(technique_name, (Technique *)NULL))
    return technique->input_layout();
  return emptyGoh;
}
*/
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
  return idx != -1 ? GraphicsObjectHandle(type, 0, idx) : emptyGoh;
}

void Graphics::fill_system_resource_views(const ResourceViewArray &views, TextureArray *out) const {

  for (size_t i = 0; i < views.size(); ++i) {
    if (views[i].used && views[i].source == PropertySource::kSystem) {
      (*out)[i] = PROPERTY_MANAGER.get_property<GraphicsObjectHandle>(views[i].class_id);
    }
  }
}

void Graphics::destroy_deferred_context(DeferredContext *ctx) {
  if (ctx) {
    if (!ctx->_is_immediate_context)
      ctx->_ctx->Release();
    delete exch_null(ctx);
  }
}

DeferredContext *Graphics::create_deferred_context(bool can_use_immediate) {
  DeferredContext *dc = new DeferredContext;
  if (can_use_immediate) {
    dc->_is_immediate_context = true;
    dc->_ctx = _immediate_context;
  } else {
    _device->CreateDeferredContext(0, &dc->_ctx);
  }

  return dc;
}

void Graphics::get_predefined_geometry(PredefinedGeometry geom, GraphicsObjectHandle *vb, int *vertex_size, GraphicsObjectHandle *ib, 
                                       DXGI_FORMAT *index_format, int *index_count) {

  *index_format = DXGI_FORMAT_R16_UINT;
  KASSERT(_predefined_geometry.find(geom) != _predefined_geometry.end());

  switch (geom) {
    case kGeomFsQuadPos:
      *vb = _predefined_geometry[kGeomFsQuadPos].first;
      *ib = _predefined_geometry[kGeomFsQuadPos].second;
      *vertex_size = sizeof(XMFLOAT3);
      *index_count = 6;
      break;

    case kGeomFsQuadPosTex:
      *vb = _predefined_geometry[kGeomFsQuadPosTex].first;
      *ib = _predefined_geometry[kGeomFsQuadPosTex].second;
      *vertex_size = sizeof(PosTex);
      *index_count = 6;
      break;
  }
}

void Graphics::add_command_list(ID3D11CommandList *cmd_list) {
  _immediate_context->ExecuteCommandList(cmd_list, FALSE);
  cmd_list->Release();
}
