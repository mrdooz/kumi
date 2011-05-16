#ifndef _DX_UTILS_HPP_
#define _DX_UTILS_HPP_

HRESULT create_dynamic_vertex_buffer(ID3D11Device *device, uint32_t vertex_count, uint32_t vertex_size, ID3D11Buffer** vertex_buffer);

struct CD3D11_INPUT_ELEMENT_DESC : public D3D11_INPUT_ELEMENT_DESC
{
	CD3D11_INPUT_ELEMENT_DESC(LPCSTR name, UINT index, DXGI_FORMAT fmt, UINT slot, 
		UINT byte_offset = D3D11_APPEND_ALIGNED_ELEMENT, 
		D3D11_INPUT_CLASSIFICATION slot_class = D3D11_INPUT_PER_VERTEX_DATA, 
		UINT step_rate = 0)
	{
		SemanticName = name;
		SemanticIndex = index;
		Format = fmt;
		InputSlot = slot;
		AlignedByteOffset = byte_offset;
		InputSlotClass = slot_class;
		InstanceDataStepRate = step_rate;
	}
};

void set_vb(ID3D11DeviceContext *context, ID3D11Buffer *buf, uint32_t stride);

#endif
