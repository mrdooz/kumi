#include "stdafx.h"
#include "graphics.hpp"
#include "logger.hpp"
#include "technique.hpp"
#include "material_manager.hpp"
#include "file_watcher.hpp"
#include "io.hpp"
#include "app.hpp"

using namespace std;

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


static_assert(sizeof(GraphicsObjectHandle) == sizeof(uint32_t), "Invalid size for GraphicsObjectHandle");

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
	, _vs_profile("vs_4_0")
	, _ps_profile("ps_4_0")
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

bool Graphics::create_render_target(int width, int height, RenderTargetData *out)
{
	out->reset();

	// create the render target
	ZeroMemory(&out->texture_desc, sizeof(out->texture_desc));
	out->texture_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
	B_WRN_HR(_device->CreateTexture2D(&out->texture_desc, NULL, &out->texture.p));

	// create the render target view
	out->rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
	B_WRN_HR(_device->CreateRenderTargetView(out->texture, &out->rtv_desc, &out->rtv.p));

	// create the depth stencil texture
	out->depth_buffer_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
	B_WRN_HR(_device->CreateTexture2D(&out->depth_buffer_desc, NULL, &out->depth_buffer.p));

	// create depth stencil view
	out->dsv_desc = CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT);
	B_WRN_HR(_device->CreateDepthStencilView(out->depth_buffer, &out->dsv_desc, &out->dsv.p));

	// create the shader resource view
	out->srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, out->texture_desc.Format);
	B_WRN_HR(_device->CreateShaderResourceView(out->texture, &out->srv_desc, &out->srv.p));

	return true;
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
	B_ERR_HR(_immediate_context._context->Map(out->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource));
	uint8_t *src = (uint8_t *)data;
	uint8_t *dst = (uint8_t *)resource.pData;
	const int w = std::min<int>(width, data_width);
	const int h = std::min<int>(height, data_height);
	for (int i = 0; i < h; ++i) {
		memcpy(dst, src, w);
		src += data_pitch;
		dst += resource.RowPitch;
	}
	_immediate_context._context->Unmap(out->texture, 0);
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
	B_ERR_HR(_swap_chain->GetBuffer(0, IID_PPV_ARGS(&_back_buffer)));
  D3D11_TEXTURE2D_DESC back_buffer_desc;
  _back_buffer->GetDesc(&back_buffer_desc);

  B_ERR_HR(_device->CreateRenderTargetView(_back_buffer, NULL, &_render_target_view));

  // depth buffer
  B_ERR_HR(_device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, width, height, 1, 1, D3D11_BIND_DEPTH_STENCIL), NULL, &_depth_stencil));
  B_ERR_HR(_device->CreateDepthStencilView(_depth_stencil, NULL, &_depth_stencil_view));

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
		adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &sd, &_swap_chain, &_device, &_feature_level, &_immediate_context._context.p));

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
	ID3D11RenderTargetView* render_targets[] = { _render_target_view };
	_immediate_context._context->OMSetRenderTargets(1, render_targets, _depth_stencil_view);
	_immediate_context._context->RSSetViewports(1, &_viewport);
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
	_immediate_context._context->ClearRenderTargetView(_render_target_view, &c.x);
	_immediate_context._context->ClearDepthStencilView(_depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
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

	const int idx = _constant_buffers.find_free_index();
	if (idx != -1 && SUCCEEDED(create_buffer_inner(_device, desc, NULL, &_constant_buffers[idx])))
			return GraphicsObjectHandle(GraphicsObjectHandle::kConstantBuffer, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_static_vertex_buffer(uint32_t data_size, const void* data) {
	const int idx = _vertex_buffers.find_free_index();
	if (idx != -1 && SUCCEEDED(create_static_vertex_buffer(data_size, 1, data, &_vertex_buffers[idx])))
			return GraphicsObjectHandle(GraphicsObjectHandle::kVertexBuffer, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_static_index_buffer(uint32_t data_size, const void* data) {
	const int idx = _index_buffers.find_free_index();
	if (idx != -1 && SUCCEEDED(create_static_index_buffer(data_size, 1, data, &_index_buffers[idx])))
			return GraphicsObjectHandle(GraphicsObjectHandle::kIndexBuffer, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_input_layout(const D3D11_INPUT_ELEMENT_DESC *desc, int elems, void *shader_bytecode, int len) {
	const int idx = _input_layouts.find_free_index();
	if (idx != -1 && SUCCEEDED(_device->CreateInputLayout(desc, elems, shader_bytecode, len, &_input_layouts[idx])))
			return GraphicsObjectHandle(GraphicsObjectHandle::kInputLayout, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_vertex_shader(void *shader_bytecode, int len, const string &id) {

	int idx = _vertex_shaders.find_by_token(id);
	if (idx != -1 || (idx = _vertex_shaders.find_free_index()) != -1) {
		ID3D11VertexShader *vs = nullptr;
		if (SUCCEEDED(_device->CreateVertexShader(shader_bytecode, len, NULL, &vs))) {
			SAFE_RELEASE(_vertex_shaders[idx].first);
			_vertex_shaders[idx].first = vs;
			return GraphicsObjectHandle(GraphicsObjectHandle::kVertexShader, 0, idx);
		}
	}
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_pixel_shader(void *shader_bytecode, int len, const string &id) {

	int idx = _pixel_shaders.find_by_token(id);
	if (idx != -1 || (idx = _pixel_shaders.find_free_index()) != -1) {
		ID3D11PixelShader *ps = nullptr;
		if (SUCCEEDED(_device->CreatePixelShader(shader_bytecode, len, NULL, &ps))) {
			SAFE_RELEASE(_pixel_shaders[idx].first);
			_pixel_shaders[idx].first = ps;
			return GraphicsObjectHandle(GraphicsObjectHandle::kPixelShader, 0, idx);
		}
	}
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_rasterizer_state(const D3D11_RASTERIZER_DESC &desc) {
	const int idx = _rasterizer_states.find_free_index();
	if (idx != -1 && SUCCEEDED(_device->CreateRasterizerState(&desc, &_rasterizer_states[idx])))
		return GraphicsObjectHandle(GraphicsObjectHandle::kRasterizerState, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_blend_state(const D3D11_BLEND_DESC &desc) {
	const int idx = _blend_states.find_free_index();
	if (idx != -1 && SUCCEEDED(_device->CreateBlendState(&desc, &_blend_states[idx])))
		return GraphicsObjectHandle(GraphicsObjectHandle::kBlendState, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC &desc) {
	const int idx = _depth_stencil_states.find_free_index();
	if (idx != -1 && SUCCEEDED(_device->CreateDepthStencilState(&desc, &_depth_stencil_states[idx])))
		return GraphicsObjectHandle(GraphicsObjectHandle::kDepthStencilState, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::create_sampler_state(const D3D11_SAMPLER_DESC &desc) {
	const int idx = _sampler_states.find_free_index();
	if (idx != -1 && SUCCEEDED(_device->CreateSamplerState(&desc, &_sampler_states[idx])))
		return GraphicsObjectHandle(GraphicsObjectHandle::kSamplerState, 0, idx);
	return GraphicsObjectHandle();
}

GraphicsObjectHandle Graphics::load_technique(const char *filename) {
	GraphicsObjectHandle ret;
	if (!APP.io()->is_watchable()) {
		// TODO: ...

	} else {
		FILE_WATCHER.add_file_watch(filename, NULL, true, threading::kMainThread, 

			[&](void *token, FileEvent event, const string &old_name, const string &new_name) {
				int idx = _techniques.find_by_token(old_name);
				if (idx != -1 || (idx = _techniques.find_free_index()) != -1) {
					if (Technique *technique = Technique::create_from_file(old_name.c_str())) {
						SAFE_DELETE(_techniques[idx].first);
						_techniques[idx] = make_pair(technique, filename);
						ret = GraphicsObjectHandle(GraphicsObjectHandle::kTechnique, 0, idx);
					}
				}
			}
		);
	}
	return ret;
}

Technique *Graphics::find_technique2(const char *name) {
	for (int i = 0; i < _techniques.Size; ++i) {
		if (_techniques[i].first && _techniques[i].first->name() == name)
			return _techniques[i].first;
	}
	return NULL;
}

GraphicsObjectHandle Graphics::find_technique(const char *name) {
	for (int i = 0; i < _techniques.Size; ++i) {
		if (_techniques[i].first && _techniques[i].first->name() == name)
			return GraphicsObjectHandle(GraphicsObjectHandle::kTechnique, 0, i);
	}
	return GraphicsObjectHandle();
}


void Graphics::submit_command(RenderKey key, void *data) {
	_render_commands.push_back(make_pair(key, data));
}

void Graphics::set_params(Technique *technique, Shader *shader, uint16 material_id, uint16 mesh_id) {

	ID3D11DeviceContext *ctx = _immediate_context._context;

	vector<CBuffer> &cbuffers = technique->get_cbuffers();

	for (size_t i = 0; i < shader->params.size(); ++i) {
		const ShaderParam &p = shader->params[i];
		const CBuffer &cb = cbuffers[p.cbuffer];
		switch (p.source) {
		case ShaderParam::Source::kMaterial:
			memcpy((void *)&cb.staging[p.start_offset], &PROPERTY_MANAGER.get_material_property<XMFLOAT4>(material_id, p.name.c_str()), p.size);
			break;
		case ShaderParam::Source::kMesh:
			{
				XMFLOAT4X4 mtx;
				XMStoreFloat4x4(&mtx, XMMatrixIdentity());
				memcpy((void *)&cb.staging[p.start_offset], &mtx, p.size);
			}
			break;
		case ShaderParam::Source::kSystem:
			{
				XMFLOAT4X4 mtx = PROPERTY_MANAGER.get_system_property<XMFLOAT4X4>(p.name.c_str());
				//XMStoreFloat4x4(&mtx, XMMatrixIdentity());
				memcpy((void *)&cb.staging[p.start_offset], &mtx, p.size);
			}
			break;
		case ShaderParam::Source::kUser:
			break;
		}
	}

	for (size_t i = 0; i < cbuffers.size(); ++i) {
		const CBuffer &cb = cbuffers[i];
		ID3D11Buffer *buffer = _constant_buffers.get(cb.handle);
		ctx->UpdateSubresource(buffer, 0, NULL, &cb.staging[0], 0, 0);
		if (shader->type() == Shader::kVertexShader)
			ctx->VSSetConstantBuffers(0, 1, &buffer);
		else
			ctx->PSSetConstantBuffers(0, 1, &buffer);
	}

}

void Graphics::render() {

	_immediate_context._context->RSSetViewports(1, &_viewport);

	// aaah, lambdas, thank you!
	sort(_render_commands.begin(), _render_commands.end(), [&](const RenderCmd &a, const RenderCmd &b) { return a.first.key < b.first.key; });

	// delete commands are sorted before render commands, so we can just save the
	// deleted items when we find them
	set<void *> deleted_items;

	for (size_t i = 0; i < _render_commands.size(); ++i) {
		const RenderCmd &cmd = _render_commands[i];
		switch (cmd.first.cmd) {
		case RenderKey::kDelete:
				deleted_items.insert(cmd.second);
				break;

		case RenderKey::kRenderTechnique: {
			if (deleted_items.find(cmd.second) != deleted_items.end())
				break;

			Technique *technique = (Technique *)cmd.second;

			break;
			}

		case RenderKey::kRenderMesh:
			{
				if (deleted_items.find(cmd.second) != deleted_items.end())
					break;
				const MeshRenderData *data = (const MeshRenderData *)cmd.second;
				const Material &material = MATERIAL_MANAGER.get_material(data->material_id);
				Technique *technique = _techniques.get(find_technique(material.technique.c_str()));
				Shader *vertex_shader = technique->vertex_shader();
				Shader *pixel_shader = technique->pixel_shader();

				ID3D11DeviceContext *ctx = _immediate_context._context;
				ctx->VSSetShader(_vertex_shaders.get(vertex_shader->shader), NULL, 0);
				ctx->GSSetShader(NULL, 0, 0);
				ctx->PSSetShader(_pixel_shaders.get(pixel_shader->shader), NULL, 0);

				set_vb(ctx, _vertex_buffers.get(data->vb), data->vertex_size);
				ctx->IASetIndexBuffer(_index_buffers.get(data->ib), data->index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
				ctx->IASetInputLayout(_input_layouts.get(technique->input_layout()));

				ctx->IASetPrimitiveTopology(data->topology);

				ctx->RSSetState(_rasterizer_states.get(technique->rasterizer_state()));
				ctx->OMSetDepthStencilState(_depth_stencil_states.get(technique->depth_stencil_state()), ~0);

				set_params(technique, vertex_shader, data->material_id, 0);
				set_params(technique, pixel_shader, data->material_id, 0);

				ctx->DrawIndexed(data->index_count, 0, 0);
			}
			break;
		}
	}

	_render_commands.clear();
}

