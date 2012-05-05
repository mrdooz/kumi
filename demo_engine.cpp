#include "stdafx.h"
#include "demo_engine.hpp"
#include "logger.hpp"
#include "json_writer.hpp"

using std::sort;

DemoEngine *DemoEngine::_instance = NULL;

DemoEngine& DemoEngine::instance() {
  assert(_instance);
  return *_instance;
}

DemoEngine::DemoEngine() 
  : _paused(true)
  , _inited(false)
  , _frequency(0)
  , _last_time(0)
  , _current_time(0)
  , _cur_effect(0)
  , _duration_ms(3 * 60 * 1000)
{
}

bool DemoEngine::create() {
  assert(!_instance);
  _instance = new DemoEngine;
  return true;
}

bool DemoEngine::start() {
  _paused = false;
  QueryPerformanceCounter((LARGE_INTEGER *)&_last_time);
  return true;
}

void DemoEngine::set_paused(bool pause) {
  _paused = pause;
}

bool DemoEngine::paused() const {
  return _paused;
}

void DemoEngine::set_pos(uint32 ms) {
}

uint32 DemoEngine::duration() const {
  return _duration_ms;
}

bool DemoEngine::init() {
  B_ERR_BOOL(!!QueryPerformanceFrequency((LARGE_INTEGER *)&_frequency));

  sort(_effects.begin(), _effects.end(), 
    [](const EffectInstance *a, const EffectInstance *b){ return a->_start_time < b->_start_time; });

  for (size_t i = 0; i < _effects.size(); ++i) {
    LOG_WRN_BOOL(_effects[i]->_effect->init());
    _inactive_effects.push_back(_effects[i]);
  }

  _inited = true;

  return true;
}

bool DemoEngine::tick() {
  if (_paused) {
    QueryPerformanceCounter((LARGE_INTEGER *)&_last_time);
    return true;
  }

  int64 now;
  QueryPerformanceCounter((LARGE_INTEGER *)&now);
  int64 delta = (now - _last_time);
  _current_time += delta;
  uint32 current_time_ms = uint32((1000 * _current_time) / _frequency);
  double elapsed_ms = 1000.0 * delta / _frequency;


  // check for any effects that have ended
  while (!_active_effects.empty() && _active_effects.front()->_end_time <= current_time_ms) {
    _active_effects.front()->_running = false;
    _active_effects.pop_front();
  }

  // check if any effect start now
  while (!_inactive_effects.empty() && _inactive_effects.front()->_start_time <= current_time_ms) {
    EffectInstance *e = _inactive_effects.front();
    _inactive_effects.pop_front();
    e->_running = true;
    _active_effects.push_back(e);
  }

  // calc the number of ticks to step
  const int64 ticks_per_s = 100;
  const double tmp = (double)elapsed_ms * ticks_per_s / 1000;
  const int num_ticks = (int)tmp;
  const float frac = (float)(tmp - floor(tmp));

  // tick the active effects
  for (size_t i = 0; i < _active_effects.size(); ++i) {
    EffectInstance *e = _active_effects[i];
    float global_time = current_time_ms / 1000.0f;
    float local_time = (current_time_ms - e->_start_time) / 1000.0f;
    XMFLOAT4 tt(global_time, local_time, 0, 0);
    PROPERTY_MANAGER.set_system_property("g_time", tt);
    e->_effect->update(current_time_ms, current_time_ms - e->_start_time, ticks_per_s, num_ticks, frac);
    e->_effect->render();
  }

  _last_time = now;

  return true;
}

bool DemoEngine::close() {
  delete exch_null(_instance);
  return true;
}

void DemoEngine::add_effect(Effect *effect, uint32 start_time, uint32 end_time) {
  assert(!_inited);
  _effects.push_back(new EffectInstance(effect, start_time, end_time));
}

void DemoEngine::reset_current_effect() {

}

std::string DemoEngine::get_info() {
  auto root = JsonValue::create_object();
  auto demo = JsonValue::create_object();
  root->add_key_value("demo", demo);
  demo->add_key_value("duration", JsonValue::create_int(_duration_ms));
  auto effects = JsonValue::create_array();
  for (auto it = begin(_effects); it != end(_effects); ++it) {
    auto effect = JsonValue::create_object();
    auto cur = *it;
    effect->add_key_value("name", JsonValue::create_string(cur->_effect->name()));
    effect->add_key_value("start_time", JsonValue::create_int(cur->_start_time));
    effect->add_key_value("end_time", JsonValue::create_int(cur->_end_time));
    effects->add_value(effect);
  }
  demo->add_key_value("effects", effects);

  return root->print(2);
}
