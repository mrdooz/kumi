#include "stdafx.h"
#include "graphics.hpp"
#include "logger.hpp"

using namespace std;

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "DXGUID.lib")
#pragma comment(lib, "D3D11.lib")

Graphics* Graphics::_instance = NULL;

Graphics& Graphics::instance()
{
	if (_instance == NULL)
		_instance = new Graphics();
	return *_instance;
}

Graphics::Graphics()
	: _width(-1)
	, _height(-1)
  , _clear_color(0,0,0,1)
  , _start_fps_time(0xffffffff)
  , _frame_count(0)
  , _fps(0)
{
	assert(!_instance);
}

Graphics::~Graphics()
{
}

bool Graphics::create_render_target(int width, int height, RenderTargetData *out)
{
	// create the render target
	ZeroMemory(&out->texture_desc, sizeof(out->texture_desc));
	out->texture_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
	B_WRN_DX(_device->CreateTexture2D(&out->texture_desc, NULL, &out->texture.p));

	// create the render target view
	out->rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
	B_WRN_DX(_device->CreateRenderTargetView(out->texture, &out->rtv_desc, &out->rtv.p));

	// create the depth stencil texture
	out->depth_buffer_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
	B_WRN_DX(_device->CreateTexture2D(&out->depth_buffer_desc, NULL, &out->depth_buffer.p));

	// create depth stencil view
	out->dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT);
	B_WRN_DX(_device->CreateDepthStencilView(out->depth_buffer, &out->dsv_desc, &out->dsv.p));

	// create the shader resource view
	out->srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
	B_WRN_DX(_device->CreateShaderResourceView(out->texture, &out->srv_desc, &out->srv.p));

	return true;
}

bool Graphics::create_texture(int width, int height, const D3D11_TEXTURE2D_DESC &desc, TextureData *out)
{
	// create the texture
	ZeroMemory(&out->texture_desc, sizeof(out->texture_desc));
	out->texture_desc = desc;
	B_WRN_DX(_device->CreateTexture2D(&out->texture_desc, NULL, &out->texture.p));

	// create the shader resource view
	out->srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
	B_WRN_DX(_device->CreateShaderResourceView(out->texture, &out->srv_desc, &out->srv.p));

	return true;
}

bool Graphics::create_texture(int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, TextureData *out)
{
	if (!create_texture(width, height, CD3D11_TEXTURE2D_DESC(fmt, width, height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), out))
		return false;

	D3D11_MAPPED_SUBRESOURCE resource;
	B_ERR_DX(_immediate_context->Map(out->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
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

  // release any existing buffers
  const bool existing_buffers = _back_buffer || _render_target_view;
  _back_buffer = NULL;
  _render_target_view = NULL;
  _depth_stencil = NULL;
  _depth_stencil_view = NULL;

  if (existing_buffers)
    _swap_chain->ResizeBuffers(1, width, height, _buffer_format, 0);

  // Get the dx11 back buffer
	B_ERR_DX(_swap_chain->GetBuffer(0, IID_PPV_ARGS(&_back_buffer)));
  D3D11_TEXTURE2D_DESC back_buffer_desc;
  _back_buffer->GetDesc(&back_buffer_desc);

  B_ERR_DX(_device->CreateRenderTargetView(_back_buffer, NULL, &_render_target_view));

  // depth buffer
  B_ERR_DX(_device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL), NULL, &_depth_stencil));
  B_ERR_DX(_device->CreateDepthStencilView(_depth_stencil, NULL, &_depth_stencil_view));

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
	B_ERR_DX(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&dxgi_factory));

	// Use the first adapter
	vector<CComPtr<IDXGIAdapter1> > adapters;
  UINT i = 0;
  IDXGIAdapter1 *adapter = nullptr;
	int perfhud = -1;
	for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters1(i, &adapter)); ++i) {
		adapters.push_back(adapter);
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);
		if (wcscmp(desc.Description, L"NVIDIA PerfHud") == 0)
			perfhud = i;
	}

	B_ERR_BOOL(!adapters.empty());

	adapter = perfhud != -1 ? adapters[perfhud] : adapters.front();

#ifdef _DEBUG
	const int flags = D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#else
	const int flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

	// Create the DX11 device
	B_ERR_DX(D3D11CreateDeviceAndSwapChain(
		adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &sd, &_swap_chain, &_device, &_feature_level, &_immediate_context));

	B_ERR_BOOL(_feature_level >= D3D_FEATURE_LEVEL_9_3);

  B_ERR_DX(_device->QueryInterface(IID_ID3D11Debug, (void **)&_d3d_debug));

  B_ERR_BOOL(create_back_buffers(width, height));

	B_ERR_DX(_device->CreateDepthStencilState(&CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT()), &_default_depth_stencil_state.p));
	B_ERR_DX(_device->CreateRasterizerState(&CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT()), &_default_rasterizer_state.p));
	B_ERR_DX(_device->CreateBlendState(&CD3D11_BLEND_DESC(CD3D11_DEFAULT()), &_default_blend_state.p));
  for (int i = 0; i < 4; ++i)
    _default_blend_factors[i] = 1.0f;
	return true;
}

void Graphics::set_default_render_target()
{
	ID3D11RenderTargetView* render_targets[] = { _render_target_view };
	_immediate_context->OMSetRenderTargets(1, render_targets, _depth_stencil_view);
	_immediate_context->RSSetViewports(1, &_viewport);
}

bool Graphics::close()
{
  delete this;
  _instance = NULL;
  return true;
}

void Graphics::clear()
{
  clear(_clear_color);
}

void Graphics::clear(const XMFLOAT4& c)
{
	_immediate_context->ClearRenderTargetView(_render_target_view, &c.x);
	_immediate_context->ClearDepthStencilView(_depth_stencil_view, D3D11_CLEAR_DEPTH, 1.0f, 0 );
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
