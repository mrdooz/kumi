#ifndef _DX_UTILS_HPP_
#define _DX_UTILS_HPP_

#include "logger.hpp"

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

// maps from screen space (0,0) top left, (width-1, height-1) bottom right
// to clip space (-1,+1) top left, (+1, -1) bottom right
void screen_to_clip(float x, float y, float w, float h, float *ox, float *oy);

// 0 1
// 2 3
void make_quad_clip_space(float *x, float *y, int stride_x, int stride_y, float center_x, float center_y, float width, float height);

// create 2 triangles, instead of verts for a single quad
void make_quad_clip_space_unindexed(float *x, float *y, float *tx, float *ty, int stride_x, int stride_y, int stride_tx, int stride_ty, 
	float center_x, float center_y, float width, float height);

inline DXGI_FORMAT index_size_to_format(int size) {
	if (size == 2) return DXGI_FORMAT_R16_UINT;
	if (size == 4) return DXGI_FORMAT_R32_UINT;
	LOG_WARNING("unknown index size");
	return DXGI_FORMAT_UNKNOWN;
}

inline int index_format_to_size(DXGI_FORMAT fmt) {
	if (fmt == DXGI_FORMAT_R16_UINT) return 2;
	if (fmt == DXGI_FORMAT_R32_UINT) return 4;
	LOG_WARNING("unknown index format");
	return 0;
}

#endif
