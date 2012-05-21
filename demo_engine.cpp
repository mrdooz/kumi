#include "stdafx.h"
#include "demo_engine.hpp"
#include "logger.hpp"
#include "json_utils.hpp"
#include "effect.hpp"

using namespace std;

DemoEngine *DemoEngine::_instance = NULL;

DemoEngine& DemoEngine::instance() {
  assert(_instance);
  return *_instance;
}

DemoEngine::DemoEngine() 
  : _paused(true)
  , _frequency(0)
  , _last_time(0)
  , _current_time(0)
  , _cur_effect(0)
  , _duration_ms(3 * 60 * 1000)
  , _time_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("g_time"))
{
  QueryPerformanceFrequency((LARGE_INTEGER *)&_frequency);
}

bool DemoEngine::create() {
  assert(!_instance);
  _instance = new DemoEngine;
  return true;
}

bool DemoEngine::start() {
  QueryPerformanceCounter((LARGE_INTEGER *)&_last_time);
  _last_time *= 1000;
  return true;
}

void DemoEngine::set_paused(bool pause) {
  _paused = pause;
}

bool DemoEngine::paused() const {
  return _paused;
}

void DemoEngine::set_pos(uint32 ms) {
  int64 new_pos = ms * _frequency;
  _current_time = new_pos;
  reclassify_effects();
}

uint32 DemoEngine::duration() const {
  return _duration_ms;
}

void DemoEngine::reclassify_effects() {
  uint32 current_time_ms = uint32(_current_time / _frequency);

  sort(RANGE(_effects), [](const Effect *a, const Effect *b){ return a->start_time() < b->start_time(); });
  _expired_effects.clear();
  _inactive_effects.clear();
  _active_effects.clear();

  bool started = false;
  bool ended = false;
  bool in_running_span = false;
  for (int i = 0; i < (int)_effects.size(); ++i) {
    auto e = _effects[i];
    started = current_time_ms >= e->start_time();
    ended = current_time_ms >= e->end_time();
    e->set_running(started && !ended);
    if (!started)
      _inactive_effects.push_back(e);
    else if (ended)
      _expired_effects.push_back(e);
    else
      _active_effects.push_back(e);
  }

}

bool DemoEngine::tick() {
  int64 now;
  QueryPerformanceCounter((LARGE_INTEGER *)&now);
  now *= 1000;
  int64 delta = _paused ? 0 : (now - _last_time);
  _current_time += delta;
  uint32 current_time_ms = uint32(_current_time / _frequency);
  double elapsed_ms = 1000.0 * delta / _frequency;

  // check for any effects that have ended
  while (!_active_effects.empty() && _active_effects.front()->end_time() <= current_time_ms) {
    _active_effects.front()->set_running(false);
    _expired_effects.push_back(_active_effects.front());
    _active_effects.pop_front();
  }

  // check if any effect start now
  while (!_inactive_effects.empty() && _inactive_effects.front()->start_time() <= current_time_ms) {
    auto e = _inactive_effects.front();
    _inactive_effects.pop_front();
    e->set_running(true);
    _active_effects.push_back(e);
  }

  // calc the number of ticks to step
  const int64 ticks_per_s = 100;
  const double tmp = (double)elapsed_ms * ticks_per_s / 1000;
  const int num_ticks = (int)tmp;
  const float frac = (float)(tmp - floor(tmp));

  // tick the active effects
  for (size_t i = 0; i < _active_effects.size(); ++i) {
    auto e = _active_effects[i];
    float global_time = current_time_ms / 1000.0f;
    float local_time = (current_time_ms - e->start_time()) / 1000.0f;
    XMFLOAT4 tt(global_time, local_time, 0, 0);
    PROPERTY_MANAGER.set_property(_time_id, tt);
    e->update(current_time_ms, current_time_ms - e->start_time(), ticks_per_s, num_ticks, frac);
    e->render();
  }

  _last_time = now;

  return true;
}

bool DemoEngine::close() {
  delete exch_null(_instance);
  return true;
}

bool DemoEngine::add_effect(Effect *effect, uint32 start_time, uint32 end_time) {
  effect->set_duration(start_time, end_time);
  _effects.push_back(effect);

  sort(RANGE(_effects), [](const Effect *a, const Effect *b){ return a->start_time() < b->start_time(); });

  for (size_t i = 0; i < _effects.size(); ++i) {
    LOG_WRN_BOOL(_effects[i]->init());
    _inactive_effects.push_back(_effects[i]);
  }

  return true;
}

Effect *DemoEngine::find_effect_by_name(const std::string &name) {
  auto it = find_if(RANGE(_effects), [&](const Effect *e) { return e->name() == name; });
  return it == end(_effects) ? nullptr : *it;
}

void DemoEngine::update(JsonValue::JsonValuePtr state) {
  _duration_ms = (*state)["duration"]->get_int();
  auto effects = (*state)["effects"];
  for (int i = 0; i < effects->count(); ++i) {
    auto cur = (*effects)[i];
    auto name = (*cur)["name"]->get_string();
    if (Effect *ie = find_effect_by_name(name)) {
      auto start_time = (*cur)["start_time"]->get_int();
      auto end_time = (*cur)["end_time"]->get_int();
      ie->set_duration(start_time, end_time);
    } else {
      LOG_ERROR_LN("unknown effect: %s", name.c_str());
    }
  }

  reclassify_effects();
}

JsonValue::JsonValuePtr DemoEngine::get_info() {
  auto root = JsonValue::create_object();
  auto demo = JsonValue::create_object();
  root->add_key_value("demo", demo);
  demo->add_key_value("duration", _duration_ms);
  auto effects = JsonValue::create_array();
  for (auto it = begin(_effects); it != end(_effects); ++it) {
    effects->add_value((*it)->get_info());
  }
  demo->add_key_value("effects", effects);
  return root;
}

