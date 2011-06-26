#include "stdafx.h"
#include "effect_wrapper.hpp"
#include "graphics.hpp"
#include "file_utils.hpp"

#pragma comment(lib, "d3dcompiler.lib")

EffectWrapper::EffectWrapper()
{
}

EffectWrapper::~EffectWrapper()
{
}

bool EffectWrapper::load_shaders(const char *buf, int len, const char *vs, const char *gs, const char *ps)
{
	return 
		either_or(!vs, load_inner(buf, len, vs, VertexShader)) && 
		either_or(!gs, load_inner(buf, len, gs, GeometryShader)) && 
		either_or(!ps, load_inner(buf, len, ps, PixelShader));
}

bool EffectWrapper::load_shaders(const char *filename, const char *vs, const char *gs, const char *ps)
{
	void *buf;
	size_t len;
	if (!load_file(filename, &buf, &len)) {
		LOG_WARNING_LN("error loading file: %s", filename);
		return false;
	}
	return load_shaders((const char *)buf, len, vs, gs, ps);
}

bool EffectWrapper::load_inner(const char *buf, int len, const char* entry_point, ShaderType type)
{

	ID3DBlob* error_blob = NULL;

	// Must use ps_4_0_level_9_3 or ps_4_0_level_9_1
	// Set shader version depending on feature level
	const char *vs, *ps, *gs;
	switch(Graphics::instance().feature_level()) {
	case D3D_FEATURE_LEVEL_9_1:
		vs = "vs_4_0_level_9_1";
		ps = "ps_4_0_level_9_1";
		gs = "gs_4_0_level_9_1";
		break;
	case D3D_FEATURE_LEVEL_9_2:
	case D3D_FEATURE_LEVEL_9_3:
		vs = "vs_4_0_level_9_3";
		ps = "ps_4_0_level_9_3";
		gs = "gs_4_0_level_9_3";
		break;
	default:
		vs = "vs_4_0";
		ps = "ps_4_0";
		gs = "gs_4_0";
		break;
	}

	ID3D11Device *device = Graphics::instance().device();
	switch (type)
	{
	case VertexShader:
		if (FAILED(D3DCompile(buf, len, "", NULL, NULL, entry_point, vs, D3D10_SHADER_ENABLE_STRICTNESS, 0, &_vs._blob, &error_blob))) {
			LOG_ERROR_LN("%s", error_blob->GetBufferPointer());
			return false;
		}
		B_ERR_DX(device->CreateVertexShader(_vs._blob->GetBufferPointer(), _vs._blob->GetBufferSize(), NULL, &_vs._shader));
		B_ERR_BOOL(_vs.do_reflection());
		break;

	case GeometryShader:
		if (FAILED(D3DCompile(buf, len, "", NULL, NULL, entry_point, gs, D3D10_SHADER_ENABLE_STRICTNESS, 0, &_gs._blob, &error_blob))) {
			LOG_ERROR_LN("%s", error_blob->GetBufferPointer());
			return false;
		}
		B_ERR_DX(device->CreateGeometryShader(_gs._blob->GetBufferPointer(), _gs._blob->GetBufferSize(), NULL, &_gs._shader));
		B_ERR_BOOL(_gs.do_reflection());
		break;

	case PixelShader:
		if (FAILED(D3DCompile(buf, len, "", NULL, NULL, entry_point, ps, D3D10_SHADER_ENABLE_STRICTNESS, 0, &_ps._blob, &error_blob))) {
			LOG_ERROR_LN("%s", error_blob->GetBufferPointer());
			return false;
		}
		B_ERR_DX(device->CreatePixelShader(_ps._blob->GetBufferPointer(), _ps._blob->GetBufferSize(), NULL, &_ps._shader));
		B_ERR_BOOL(_ps.do_reflection());
		break;
	}

	return true;
}


bool EffectWrapper::set_resource(const string& /*name*/, ID3D11ShaderResourceView *resource)
{
	D3D_CONTEXT->PSSetShaderResources(0, 1, &resource);
	return true;
}

void EffectWrapper::set_cbuffer()
{
	ID3D11DeviceContext* context = Graphics::instance().context();
	if (_vs._shader) {
		for (auto i = _vs._constant_buffers.begin(), e = _vs._constant_buffers.end(); i != e; ++i) {
			ConstantBuffer *b = i->second;
			if (b->_mapped) {
				b->_mapped = false;
				context->Unmap(b->_buffer, 0);
			}
			context->VSSetConstantBuffers(0, 1, &b->_buffer.p);
		}
	} 

	if (_gs._shader) {
		for (auto i = _gs._constant_buffers.begin(), e = _gs._constant_buffers.end(); i != e; ++i) {
			ConstantBuffer *b = i->second;
			if (b->_mapped) {
				b->_mapped = false;
				context->Unmap(b->_buffer, 0);
			}
			context->GSSetConstantBuffers(0, 1, &b->_buffer.p);
		}
	} 

	if (_ps._shader) {
		for (auto i = _ps._constant_buffers.begin(), e = _ps._constant_buffers.end(); i != e; ++i) {
			ConstantBuffer *b = i->second;
			if (b->_mapped) {
				b->_mapped = false;
				context->Unmap(b->_buffer, 0);
			}
			context->PSSetConstantBuffers(0, 1, &b->_buffer.p);
		}
	}
}

void EffectWrapper::unmap_buffers()
{
	_vs.unmap_buffers();
	_gs.unmap_buffers();
	_ps.unmap_buffers();
}

ID3D11InputLayout* EffectWrapper::create_input_layout(const D3D11_INPUT_ELEMENT_DESC* elems, const int num_elems)
{
	ID3D11InputLayout* layout = NULL;
	if (FAILED(GRAPHICS.device()->CreateInputLayout(elems, num_elems, _vs._blob->GetBufferPointer(), _vs._blob->GetBufferSize(), &layout)))
		return nullptr;
	return layout;
}

ID3D11InputLayout* EffectWrapper::create_input_layout(const std::vector<D3D11_INPUT_ELEMENT_DESC>& elems)
{
	return create_input_layout(&elems[0], elems.size());
}

void EffectWrapper::set_shaders(ID3D11DeviceContext *context)
{
	context->VSSetShader(vertex_shader(), NULL, 0);
	context->GSSetShader(geometry_shader(), NULL, 0);
	context->PSSetShader(pixel_shader(), NULL, 0);
}
