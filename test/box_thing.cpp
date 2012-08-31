#include "stdafx.h"
#include "box_thing.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_interface.hpp"
#include "../scene.hpp"
#include "../threading.hpp"
#include "../mesh.hpp"
#include "../tweakable_param.hpp"
#include "../graphics.hpp"
#include "../material.hpp"
#include "../material_manager.hpp"
#include "../xmath.hpp"
#include "../profiler.hpp"
#include "../deferred_context.hpp"
#include "../animation_manager.hpp"
#include "../app.hpp"
#include "../bit_utils.hpp"
#include "../dx_utils.hpp"
#include "../vertex_types.hpp"

using namespace std;
using namespace std::tr1::placeholders;

BoxThing::BoxThing(const std::string &name) 
  : Effect(name)
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
  , _ctx(nullptr)
  , _useFreeFlyCamera(false)
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

BoxThing::~BoxThing() {
  GRAPHICS.destroy_deferred_context(_ctx);
}

bool BoxThing::init() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx = GRAPHICS.create_deferred_context(true);

  return true;
}

void BoxThing::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  *proj = _freefly_camera.projectionMatrix();

  double orgDelta = delta;
  if (is_bit_set(GetAsyncKeyState(VK_SHIFT), 15))
    delta /= 10;

  if (_useFreeFlyCamera) {

    if (_keystate['W'])
      _freefly_camera.move(FreeFlyCamera::kForward, (float)(100 * delta));
    if (_keystate['S'])
      _freefly_camera.move(FreeFlyCamera::kForward, (float)(-100 * delta));
    if (_keystate['A'])
      _freefly_camera.move(FreeFlyCamera::kRight, (float)(-100 * delta));
    if (_keystate['D'])
      _freefly_camera.move(FreeFlyCamera::kRight, (float)(100 * delta));
    if (_keystate['Q'])
      _freefly_camera.move(FreeFlyCamera::kUp, (float)(100 * delta));
    if (_keystate['E'])
      _freefly_camera.move(FreeFlyCamera::kUp, (float)(-100 * delta));

    if (_mouse_lbutton) {
      float dx = (float)(100 * orgDelta) * _mouse_horiz / 200.0f;
      float dy = (float)(100 * orgDelta) * _mouse_vert / 200.0f;
      _freefly_camera.rotate(FreeFlyCamera::kXAxis, dx);
      _freefly_camera.rotate(FreeFlyCamera::kYAxis, dy);
    }
    _cameraPos = _freefly_camera.pos();

    *view = _freefly_camera.viewMatrix();
  } else {

  }
}

bool BoxThing::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time  / 1000.0;

  calc_camera_matrices(time, delta_ns / 1e6, &_view, &_proj);

  return true;
}

bool BoxThing::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx->end_frame();

  return true;
}

bool BoxThing::close() {
  return true;
}

void BoxThing::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
  case WM_KEYDOWN:
    if (wParam <= 255)
      _keystate[wParam] = 1;
    break;

  case WM_KEYUP:
    if (wParam <= 255)
      _keystate[wParam] = 0;
    switch (wParam) {
    case 'F':
      _useFreeFlyCamera = !_useFreeFlyCamera;
      break;
    case 'Z':
      //_useZFill = !_useZFill;
      break;
    }
    break;

  case WM_MOUSEMOVE:
    if (_mouse_pos_prev != ~0) {
      _mouse_horiz = LOWORD(lParam) - LOWORD(_mouse_pos_prev);
      _mouse_vert = HIWORD(lParam) - HIWORD(_mouse_pos_prev);
    }
    _mouse_pos_prev = lParam;
    break;

  case WM_LBUTTONDOWN:
    _mouse_lbutton = true;
    break;

  case WM_LBUTTONUP:
    _mouse_lbutton = false;
    break;

  case WM_RBUTTONDOWN:
    _mouse_rbutton = true;
    break;

  case WM_RBUTTONUP:
    _mouse_rbutton = false;
    break;
  }

}
