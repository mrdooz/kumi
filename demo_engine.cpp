#include "stdafx.h"
#include "demo_engine.hpp"
#include "logger.hpp"
#include "json_utils.hpp"

using std::sort;

DemoEngine *DemoEngine::_instance = NULL;

TweakableParam::TweakableParam() : _type(kTypeUnknown), _animation(kAnimUnknown), _bool(nullptr) {}

TweakableParam::TweakableParam(Type type, Animation animation) : _type(type), _animation(animation) {
  switch (_type) {
    case kTypeBool: _bool = new vector<Key<bool> >; break;
    case kTypeInt: _int = new vector<Key<int> >; break;
    case kTypeFloat: _float = new vector<Key<float> >; break;
    case kTypeFloat2: _float2 = new vector<Key<XMFLOAT2> >; break;
    case kTypeFloat3: _float3 = new vector<Key<XMFLOAT3> >; break;
    case kTypeFloat4: 
    case kTypeColor:
      _float4 = new vector<Key<XMFLOAT4> >; break;
    case kTypeString:
    case kTypeFile:  _string = new std::string; break;
  }
}

TweakableParam::~TweakableParam() {
  switch (_type) {
    case kTypeBool: SAFE_DELETE(_bool); break;
    case kTypeInt: SAFE_DELETE(_int); break;
    case kTypeFloat: SAFE_DELETE(_float); break;
    case kTypeFloat2: SAFE_DELETE(_float2); break;
    case kTypeFloat3: SAFE_DELETE(_float3); break;
    case kTypeFloat4:
    case kTypeColor: 
      SAFE_DELETE(_float4); break;
    case kTypeString: SAFE_DELETE(_string); break;
    case kTypeFile: SAFE_DELETE(_string); break;
  }
}

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
  _current_time = ms * _frequency / 1000;
}

uint32 DemoEngine::duration() const {
  return _duration_ms;
}

bool DemoEngine::init() {
  B_ERR_BOOL(!!QueryPerformanceFrequency((LARGE_INTEGER *)&_frequency));

  sort(_effects.begin(), _effects.end(), 
    [](const Effect *a, const Effect *b){ return a->start_time() < b->start_time(); });

  for (size_t i = 0; i < _effects.size(); ++i) {
    LOG_WRN_BOOL(_effects[i]->init());
    _inactive_effects.push_back(_effects[i]);
  }

  _inited = true;

  return true;
}

bool DemoEngine::tick() {
  int64 now;
  QueryPerformanceCounter((LARGE_INTEGER *)&now);
  int64 delta = _paused ? 0 : (now - _last_time);
  _current_time += delta;
  uint32 current_time_ms = uint32((1000 * _current_time) / _frequency);
  double elapsed_ms = 1000.0 * delta / _frequency;

  // check for any effects that have ended
  while (!_active_effects.empty() && _active_effects.front()->end_time() <= current_time_ms) {
    _active_effects.front()->set_running(false);
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
    PROPERTY_MANAGER.set_system_property("g_time", tt);
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

void DemoEngine::add_effect(Effect *effect, uint32 start_time, uint32 end_time) {
  assert(!_inited);
  effect->set_duration(start_time, end_time);
  _effects.push_back(effect);
}

void DemoEngine::reset_current_effect() {

}

JsonValue::JsonValuePtr DemoEngine::get_info() {
  auto root = JsonValue::create_object();
  auto demo = JsonValue::create_object();
  root->add_key_value("demo", demo);
  demo->add_key_value("duration", JsonValue::create_int(_duration_ms));
  auto effects = JsonValue::create_array();
  for (auto it = begin(_effects); it != end(_effects); ++it) {
    effects->add_value((*it)->get_info());
  }
  demo->add_key_value("effects", effects);
  return root;
}

JsonValue::JsonValuePtr Effect::get_info() const {
  auto info = JsonValue::create_object();
  info->add_key_value("name", JsonValue::create_string(name()));
  info->add_key_value("start_time", JsonValue::create_int(_start_time));
  info->add_key_value("end_time", JsonValue::create_int(_end_time));

  auto params = JsonValue::create_object();
  info->add_key_value("params", params);
  return info;
}
