#include "stdafx.h"
#include "demo_engine.hpp"
#include "logger.hpp"
#include "json_utils.hpp"
#include "effect.hpp"
#include "profiler.hpp"

using namespace std;

DemoEngine *DemoEngine::_instance = NULL;

DemoEngine& DemoEngine::instance() {
  KASSERT(_instance);
  return *_instance;
}

DemoEngine::DemoEngine() 
  : _paused(true)
  , _frequency(0)
  , _last_time_ctr(0)
  , _active_time_ctr(0)
  , _running_time_ctr(0)
  , _cur_effect(0)
  , _duration_ns(3 * 60 * 1000 * 1000)
  , _time_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("System::g_time"))
{
  QueryPerformanceFrequency((LARGE_INTEGER *)&_frequency);
}

DemoEngine::~DemoEngine() {
  seq_delete(&_effects);
  _active_effects.clear();
  _inactive_effects.clear();
  _expired_effects.clear();
}

bool DemoEngine::create() {
  KASSERT(!_instance);
  _instance = new DemoEngine;
  return true;
}

bool DemoEngine::start() {
  QueryPerformanceCounter((LARGE_INTEGER *)&_last_time_ctr);
  set_paused(false);
  return true;
}

void DemoEngine::set_paused(bool pause) {
  _paused = pause;
}

bool DemoEngine::paused() const {
  return _paused;
}

void DemoEngine::set_pos(int64 ms) {
  _active_time_ctr = max(0, ms * _frequency / 1000);
  reclassify_effects();
}

int64 DemoEngine::pos() {
  return ctr_to_ns(_active_time_ctr) / 1000;
}

int64 DemoEngine::duration() const {
  return _duration_ns / 1000;
}

int64 DemoEngine::ctr_to_ns(int64 ctr) {
  // convert QueryPerformanceCounter to ns
  return 1000000 * ctr / _frequency;
}

void DemoEngine::reclassify_effects() {
  int64 current_time_ns = ctr_to_ns(_active_time_ctr);
  //uint32 current_time_ms = uint32(_active_time / (1000 * _frequency));

  sort(RANGE(_effects), [](const Effect *a, const Effect *b){ return a->start_time() < b->start_time(); });
  _expired_effects.clear();
  _inactive_effects.clear();
  _active_effects.clear();

  bool started = false;
  bool ended = false;
  bool in_running_span = false;
  for (int i = 0; i < (int)_effects.size(); ++i) {
    auto e = _effects[i];
    started = current_time_ns >= 1000 * e->start_time();
    ended = current_time_ns >= 1000 * e->end_time();
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
  ADD_PROFILE_SCOPE();
  int64 now_ctr;
  QueryPerformanceCounter((LARGE_INTEGER *)&now_ctr);
  int64 now_ns = ctr_to_ns(now_ctr);
  int64 delta_ctr = now_ctr - _last_time_ctr;
  int64 delta_ns = ctr_to_ns(now_ctr - _last_time_ctr);
  int64 active_delta_ns = _paused ? 0 : delta_ns;
  _active_time_ctr += _paused ? 0 : delta_ctr;
  _running_time_ctr += delta_ctr;

  int64 current_time_ns = ctr_to_ns(_active_time_ctr);
  int64 current_time_ms = current_time_ns / 1000;

  // check for any effects that have ended
  while (!_active_effects.empty() && _active_effects.front()->end_time() * 1000 <= current_time_ns) {
    _active_effects.front()->set_running(false);
    _expired_effects.push_back(_active_effects.front());
    _active_effects.pop_front();
  }

  vector<Effect *> firstTimers;

  // check if any effect start now
  while (!_inactive_effects.empty() && _inactive_effects.front()->start_time() * 1000 <= current_time_ns) {
    auto e = _inactive_effects.front();
    _inactive_effects.pop_front();
    e->set_running(true);
    _active_effects.push_back(e);
    firstTimers.push_back(e);
  }

  // calc the number of ticks to step
  const int64 update_freq = 100;
  const float real_num_ticks = (float)(active_delta_ns * update_freq / 1e6);
  const int int_num_ticks = (int)real_num_ticks;
  const float frac_num_ticks = real_num_ticks - int_num_ticks;

  // tick the active effects
  for (size_t i = 0; i < _active_effects.size(); ++i) {
    auto *e = _active_effects[i];
    float local_time = (current_time_ns - e->start_time()) / 1e6f;

    if (find(begin(firstTimers), end(firstTimers), e) != end(firstTimers)) {
      e->set_start_time(current_time_ms);
      e->updateBase(current_time_ms, 0, 0, _paused, update_freq, 1, 0);
    } else {
      e->updateBase(current_time_ms, current_time_ms - e->start_time(), delta_ns, _paused, 
        update_freq, int_num_ticks, frac_num_ticks);
    }

    e->render();
  }

  _last_time_ctr = now_ctr;

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

void DemoEngine::update(const JsonValue::JsonValuePtr &state) {
  _duration_ns = 1000 * (*state)["duration"]->to_int();
  auto effects = (*state)["effects"];
  for (int i = 0; i < effects->count(); ++i) {
    auto cur = (*effects)[i];
    auto name = (*cur)["name"]->to_string();
    if (Effect *ie = find_effect_by_name(name)) {
      ie->update_from_json(cur);
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
  demo->add_key_value("duration", (int)(_duration_ns / 1000));
  auto effects = JsonValue::create_array();
  for (auto it = begin(_effects); it != end(_effects); ++it) {
    effects->add_value((*it)->get_info());
  }
  demo->add_key_value("effects", effects);
  return root;
}


void DemoEngine::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {
  for (size_t i = 0; i < _active_effects.size(); ++i) {
    _active_effects[i]->wndProcBase(message, wParam, lParam);
  }
}
