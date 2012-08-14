#include "stdafx.h"
#include "particle_test.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_manager.hpp"
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

using namespace std;
using namespace std::tr1::placeholders;

static const int KERNEL_SIZE = 32;
static const int NOISE_SIZE = 16;

ParticleTest::ParticleTest(const std::string &name) 
  : Effect(name)
  , _scene(nullptr) 
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::proj"))
  , _use_freefly_camera(true)
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
  , _ctx(nullptr)
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

ParticleTest::~ParticleTest() {
  GRAPHICS.destroy_deferred_context(_ctx);
  delete exch_null(_scene);
}

bool ParticleTest::file_changed(const char *filename, void *token) {
  return true;
}

bool ParticleTest::init() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/particles.tec", true));
  _particle_technique = GRAPHICS.find_technique("particles");

  _ctx = GRAPHICS.create_deferred_context(true);
/*
    {
      TweakableParameterBlock block("blur");
      block._params.emplace_back(TweakableParameter("blurX", 10.0f, 1.0f, 250.0f));
      block._params.emplace_back(TweakableParameter("blurY", 10.0f, 1.0f, 250.0f));

      block._params.emplace_back(TweakableParameter("nearFocusStart", 10.0f, 1.0f, 2500.0f));
      block._params.emplace_back(TweakableParameter("nearFocusEnd", 50.0f, 1.0f, 2500.0f));
      block._params.emplace_back(TweakableParameter("farFocusStart", 100.0f, 1.0f, 2500.0f));
      block._params.emplace_back(TweakableParameter("farFocusEnd", 150.0f, 1.0f, 2500.0f));

      APP.add_parameter_block(block, true, [=](const JsonValue::JsonValuePtr &msg) {
        _blurX = (float)msg->get("blurX")->get("value")->to_number();
        _blurY = (float)msg->get("blurY")->get("value")->to_number();

        if (msg->has_key("nearFocusStart")) {
          _nearFocusStart = (float)msg->get("nearFocusStart")->get("value")->to_number();
          _nearFocusEnd = (float)msg->get("nearFocusEnd")->get("value")->to_number();
          _farFocusStart = (float)msg->get("farFocusStart")->get("value")->to_number();
          _farFocusEnd = (float)msg->get("farFocusEnd")->get("value")->to_number();
        }
      });
    }
    }
*/

  return true;
}

void ParticleTest::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  float fov = 45 * 3.1415f / 180;
  float aspect = 1.6f;
  float zn = 1;
  float zf = 2500;
  *proj = transpose(perspective_foh(fov, aspect, zn, zf));


  if (!is_bit_set(GetKeyState(VK_CONTROL), 15)) {
    if (_keystate['W'])
      _freefly_camera.pos += (float)(100 * delta) * _freefly_camera.dir;
    if (_keystate['S'])
      _freefly_camera.pos -= (float)(100 * delta) * _freefly_camera.dir;
    if (_keystate['A'])
      _freefly_camera.pos -= (float)(100 * delta) * _freefly_camera.right;
    if (_keystate['D'])
      _freefly_camera.pos += (float)(100 * delta) * _freefly_camera.right;
    if (_keystate['Q'])
      _freefly_camera.pos += (float)(100 * delta) * _freefly_camera.up;
    if (_keystate['E'])
      _freefly_camera.pos -= (float)(100 * delta) * _freefly_camera.up;
  }

  if (_mouse_lbutton) {
    float dx = _mouse_horiz / 200.0f;
    float dy = _mouse_vert / 200.0f;
    _freefly_camera.rho -= dx;
    _freefly_camera.theta += dy;
  }

  _freefly_camera.dir = vec3_from_spherical(_freefly_camera.theta, _freefly_camera.rho);
  //_freefly_camera.up = drop(XMFLOAT4(0,1,0,0) * mtx_from_axis_angle(_freefly_camera.dir, _freefly_camera.roll));
  _freefly_camera.right = cross(_freefly_camera.up, _freefly_camera.dir);
  _freefly_camera.up = cross(_freefly_camera.dir, _freefly_camera.right);

  // Camera mtx = inverse of camera -> world mtx
  // R^-1         0
  // -R^-1 * t    1

  XMFLOAT4X4 tmp(mtx_identity());
  set_col(_freefly_camera.right, 0, &tmp);
  set_col(_freefly_camera.up, 1, &tmp);
  set_col(_freefly_camera.dir, 2, &tmp);

  tmp._41 = -dot(_freefly_camera.right, _freefly_camera.pos);
  tmp._42 = -dot(_freefly_camera.up, _freefly_camera.pos);
  tmp._43 = -dot(_freefly_camera.dir, _freefly_camera.pos);

  *view = transpose(tmp);
}

bool ParticleTest::update(int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time  / 1000.0;

  calc_camera_matrices(time, (double)delta / 1000000.0, &_view, &_proj);

  PROPERTY_MANAGER.set_property(_view_mtx_id, _view);
  PROPERTY_MANAGER.set_property(_proj_mtx_id, _proj);

  return true;
}

bool ParticleTest::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx->end_frame();

  return true;
}

bool ParticleTest::close() {
  return true;
}

void ParticleTest::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

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
      _use_freefly_camera = !_use_freefly_camera;
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
