#include "stdafx.h"
#include "dx_utils.hpp"
#include "vertex_types.hpp"
#include "graphics.hpp"

void screen_to_clip(float x, float y, float w, float h, float *ox, float *oy)
{
  *ox = (x - w / 2) / (w / 2);
  *oy = (h/2 - y) / (h/2);
}

void make_quad_clip_space(float *x, float *y, int stride_x, int stride_y, float center_x, float center_y, float width, float height)
{
  // 0 1
  // 2 3
  uint8_t *px = (uint8_t *)x;
  uint8_t *py = (uint8_t *)y;

  *(float*)&px[0*stride_x] = center_x - width;
  *(float*)&py[0*stride_y] = center_y + height;

  *(float*)&px[1*stride_x] = center_x + width;
  *(float*)&py[1*stride_y] = center_y + height;

  *(float*)&px[2*stride_x] = center_x - width;
  *(float*)&py[2*stride_y] = center_y - height;

  *(float*)&px[3*stride_x] = center_x + width;
  *(float*)&py[3*stride_y] = center_y - height;
}

void make_quad_clip_space_unindexed(float *x, float *y, float *tx, float *ty, int stride_x, int stride_y, int stride_tx, int stride_ty, 
                                    float center_x, float center_y, float width, float height)
{
#define X(n) *x = v ## n.pos.x; x = (float *)((uint8_t *)x + stride_x);
#define Y(n) *y = v ## n.pos.y; y = (float *)((uint8_t *)y + stride_y);
#define U(n) if (tx) { *tx = v ## n.tex.x; tx = (float *)((uint8_t *)tx + stride_tx); }
#define V(n) if (ty) { *ty = v ## n.tex.y; ty = (float *)((uint8_t *)ty + stride_ty); }

  // 0 1
  // 2 3
  PosTex v0(center_x - width, center_y + height, 0, 0, 0);
  PosTex v1(center_x + width, center_y + height, 0, 1, 0);
  PosTex v2(center_x - width, center_y - height, 0, 0, 1);
  PosTex v3(center_x + width, center_y - height, 0, 1, 1);

  // 2, 0, 1
  X(2); Y(2); U(2); V(2);
  X(0); Y(0); U(0); V(0);
  X(1); Y(1); U(1); V(1);

  // 2, 1, 3
  X(2); Y(2); U(2); V(2);
  X(1); Y(1); U(1); V(1);
  X(3); Y(3); U(3); V(3);

#undef X
#undef Y
#undef U
#undef V

}


ScopedRt::ScopedRt(int width, int height, DXGI_FORMAT format, uint32 bufferFlags, const std::string &name) 
  : _handle(GRAPHICS.get_temp_render_target(FROM_HERE, width, height, format, bufferFlags, name))
#if _DEBUG
  , _name(name)
#endif
{
}

ScopedRt::~ScopedRt() {
  GRAPHICS.release_temp_render_target(_handle);
}

ScopedRt::operator GraphicsObjectHandle() {
  return _handle;
}
