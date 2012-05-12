#include "stdafx.h"
#include "demo_engine.hpp"
#include "logger.hpp"
#include "json_utils.hpp"

using std::sort;

DemoEngine *DemoEngine::_instance = NULL;

//TweakableParam::TweakableParam() : _type(kTypeUnknown), _animation(kAnimUnknown), _bool(nullptr) {}

TweakableParam::TweakableParam(const std::string &name, Type type, Animation animation) 
  : _name(name)
  , _type(type)
  , _animation(animation) {
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
  , _frequency(0)
  , _last_time(0)
  , _current_time(0)
  , _cur_effect(0)
  , _duration_ms(3 * 60 * 1000)
  ,_forced_negative_update(false)
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
  return true;
}

void DemoEngine::set_paused(bool pause) {
  _paused = pause;
}

bool DemoEngine::paused() const {
  return _paused;
}

void DemoEngine::set_pos(uint32 ms) {
  int64 new_pos = ms * _frequency / 1000;
  _forced_negative_update = new_pos < _current_time;
  _current_time = new_pos;
}

uint32 DemoEngine::duration() const {
  return _duration_ms;
}
/*
bool DemoEngine::init() {

  sort(_effects.begin(), _effects.end(), 
    [](const Effect *a, const Effect *b){ return a->start_time() < b->start_time(); });

  for (size_t i = 0; i < _effects.size(); ++i) {
    LOG_WRN_BOOL(_effects[i]->init());
    _inactive_effects.push_back(_effects[i]);
  }

  _inited = true;

  return true;
}
*/
bool DemoEngine::tick() {
  int64 now;
  QueryPerformanceCounter((LARGE_INTEGER *)&now);
  int64 delta = _paused ? 0 : (now - _last_time);
  _current_time += delta;
  uint32 current_time_ms = uint32((1000 * _current_time) / _frequency);
  double elapsed_ms = 1000.0 * delta / _frequency;

  // if we've been forcefully moved to a previous time, check if we need to reinit any effects
  if (_forced_negative_update ){
    _forced_negative_update = false;
    for (auto it = begin(_expired_effects); it != end(_expired_effects); ) {
      if (current_time_ms >= (*it)->start_time() && current_time_ms < (*it)->end_time()) {
        _inactive_effects.push_back(*it);
        it = _expired_effects.erase(it);
      } else {
        ++it;
      }
    }
  }

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

std::string type_to_string(TweakableParam::Type type) {
  switch (type) {
    case TweakableParam::kTypeUnknown: return "unknown";
    case TweakableParam::kTypeBool: return "bool";
    case TweakableParam::kTypeInt: return "int";
    case TweakableParam::kTypeFloat: return "float";
    case TweakableParam::kTypeFloat2: return "float2";
    case TweakableParam::kTypeFloat3: return "float3";
    case TweakableParam::kTypeFloat4: return "float4";
    case TweakableParam::kTypeColor: return "color";
    case TweakableParam::kTypeString: return "string";
    case TweakableParam::kTypeFile: return "file";
  }
  return "";
}

std::string anim_to_string(TweakableParam::Animation anim) {
  switch (anim) {
    case TweakableParam::kAnimUnknown: return "unknown";
    case TweakableParam::kAnimStatic: return "static";
    case TweakableParam::kAnimStep: return "step";
    case TweakableParam::kAnimLinear: return "linear";
    case TweakableParam::kAnimSpline: return "spline";
  }
  return "";
}


JsonValue::JsonValuePtr get_keys(const TweakableParam *p) {
  auto a = JsonValue::create_array();
  auto type = p->_type;

  switch (type) {

    case TweakableParam::kTypeBool: {
      for (auto it = begin(*p->_bool); it != end(*p->_bool); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", JsonValue::create_int(it->time));
        o->add_key_value("value", JsonValue::create_bool(it->value));
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeInt: {
      for (auto it = begin(*p->_int); it != end(*p->_int); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", JsonValue::create_int(it->time));
        o->add_key_value("value", JsonValue::create_int(it->value));
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeFloat: {
      for (auto it = begin(*p->_float); it != end(*p->_float); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", JsonValue::create_int(it->time));
        auto v = JsonValue::create_object();
        v->add_key_value("x", JsonValue::create_number(it->value));
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeFloat2: {
      for (auto it = begin(*p->_float2); it != end(*p->_float2); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", JsonValue::create_int(it->time));
        auto v = JsonValue::create_object();
        v->add_key_value("x", JsonValue::create_number(it->value.x));
        v->add_key_value("y", JsonValue::create_number(it->value.y));
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeFloat3: {
      for (auto it = begin(*p->_float3); it != end(*p->_float3); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", JsonValue::create_int(it->time));
        auto v = JsonValue::create_object();
        v->add_key_value("x", JsonValue::create_number(it->value.x));
        v->add_key_value("y", JsonValue::create_number(it->value.y));
        v->add_key_value("z", JsonValue::create_number(it->value.z));
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeColor:
    case TweakableParam::kTypeFloat4: {
      for (auto it = begin(*p->_float4); it != end(*p->_float4); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", JsonValue::create_int(it->time));
        auto v = JsonValue::create_object();
        v->add_key_value(type == TweakableParam::kTypeColor ? "r" : "x", JsonValue::create_number(it->value.x));
        v->add_key_value(type == TweakableParam::kTypeColor ? "g" : "y", JsonValue::create_number(it->value.y));
        v->add_key_value(type == TweakableParam::kTypeColor ? "b" : "z", JsonValue::create_number(it->value.z));
        v->add_key_value(type == TweakableParam::kTypeColor ? "a" : "w", JsonValue::create_number(it->value.w));
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeString:
    case TweakableParam::kTypeFile: {
      // these guys can only have a single key
      auto o = JsonValue::create_object();
      o->add_key_value("time", JsonValue::create_int(0));
      o->add_key_value("value", JsonValue::create_string(*p->_string));
      a->add_value(o);
      break;
    }
  }

  return a;
}

JsonValue::JsonValuePtr add_param(const TweakableParam *param) {
  auto obj = JsonValue::create_object();
  obj->add_key_value("name", JsonValue::create_string(param->_name));
  obj->add_key_value("type", JsonValue::create_string(type_to_string(param->_type)));
  obj->add_key_value("anim", JsonValue::create_string(anim_to_string(param->_animation)));
  obj->add_key_value("keys", get_keys(param));

  return obj;
}

JsonValue::JsonValuePtr Effect::get_info() const {
  auto info = JsonValue::create_object();
  info->add_key_value("name", JsonValue::create_string(name()));
  info->add_key_value("start_time", JsonValue::create_int(_start_time));
  info->add_key_value("end_time", JsonValue::create_int(_end_time));

  auto params = JsonValue::create_array();
  for (auto it = begin(_params); it != end(_params); ++it) {
    params->add_value(add_param((*it).get()));
  }
  info->add_key_value("params", params);
  return info;
}
