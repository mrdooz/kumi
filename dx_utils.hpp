#ifndef _DX_UTILS_HPP_
#define _DX_UTILS_HPP_

HRESULT create_dynamic_vertex_buffer(ID3D11Device *device, const uint32_t vertex_count, const uint32_t vertex_size, ID3D11Buffer** vertex_buffer);

#endif
