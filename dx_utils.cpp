#include "stdafx.h"
#include "dx_utils.hpp"

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

HRESULT create_static_vertex_buffer(ID3D11Device* device, const uint32_t vertex_count, const uint32_t vertex_size, const void* data, ID3D11Buffer** vertex_buffer) 
{
	return create_buffer_inner(device, CD3D11_BUFFER_DESC(vertex_count * vertex_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE), data, vertex_buffer);
}

HRESULT create_dynamic_vertex_buffer(ID3D11Device *device, const uint32_t vertex_count, const uint32_t vertex_size, ID3D11Buffer** vertex_buffer)
{
	return create_buffer_inner(device, CD3D11_BUFFER_DESC(vertex_count * vertex_size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE), NULL, vertex_buffer);
}

