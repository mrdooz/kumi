#include "stdafx.h"
#include "effect.hpp"
#include "tweakable_param.hpp"
#include "bit_utils.hpp"

Effect::Effect(const std::string &name) 
  : _name(name)
  , _running(false)
  , _start_time(~0)
  , _end_time(~0)
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
  , _useFreeflyCamera(true)
{
}

Effect::~Effect() {
}

bool Effect::init() {
  return true; 
}

bool Effect::reset() { 
  return true; 
}

bool Effect::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) { 
  return true; 
}

bool Effect::render() { 
  return true; 
}

bool Effect::close() { 
  return true; 
}

JsonValue::JsonValuePtr Effect::get_info() const {
  auto info = JsonValue::create_object();
  info->add_key_value("name", name());
  info->add_key_value("start_time", (int32)_start_time);
  info->add_key_value("end_time", (int32)_end_time);

  auto params = JsonValue::create_array();
  for (auto it = begin(_params); it != end(_params); ++it) {
    params->add_value((*it)->to_json());
  }
  info->add_key_value("params", params);
  return info;
}

void Effect::update_from_json(const JsonValue::JsonValuePtr &state) {

  _start_time = (*state)["start_time"]->to_int();
  _end_time = (*state)["end_time"]->to_int();
}

void Effect::set_duration(int64 start_time, int64 end_time) {
  _start_time = start_time;
  _end_time = end_time;
}

void Effect::set_start_time(int64 startTime) {
  _start_time = startTime;
}

bool Effect::updateBase(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {

  updateFreeflyCamera(local_time / 1e3, delta_ns / 1e6);
  return update(global_time, local_time, delta_ns, paused, frequency, num_ticks, ticks_fraction);
}

void Effect::wndProcBase(UINT message, WPARAM wParam, LPARAM lParam) {

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
      _useFreeflyCamera = !_useFreeflyCamera;
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

  wnd_proc(message, wParam, lParam);
}

void Effect::updateFreeflyCamera(double time, double delta) {

  if (!_useFreeflyCamera)
    return;

  double orgDelta = delta;
  if (is_bit_set(GetAsyncKeyState(VK_SHIFT), 15))
    delta /= 10;

  if (_keystate['W'])
    _freefly_camera.move(FreeflyCamera::kForward, (float)(100 * delta));
  if (_keystate['S'])
    _freefly_camera.move(FreeflyCamera::kForward, (float)(-100 * delta));
  if (_keystate['A'])
    _freefly_camera.move(FreeflyCamera::kRight, (float)(-100 * delta));
  if (_keystate['D'])
    _freefly_camera.move(FreeflyCamera::kRight, (float)(100 * delta));
  if (_keystate['Q'])
    _freefly_camera.move(FreeflyCamera::kUp, (float)(100 * delta));
  if (_keystate['E'])
    _freefly_camera.move(FreeflyCamera::kUp, (float)(-100 * delta));

  if (_mouse_lbutton) {
    float dx = (float)(100 * orgDelta) * _mouse_horiz / 200.0f;
    float dy = (float)(100 * orgDelta) * _mouse_vert / 200.0f;
    _freefly_camera.rotate(FreeflyCamera::kXAxis, dx);
    _freefly_camera.rotate(FreeflyCamera::kYAxis, dy);
  }
}
