#include "stdafx.h"
#include "effect.hpp"
#include "tweakable_param.hpp"

Effect::Effect(const std::string &name) 
  : _name(name), _running(false), _start_time(~0), _end_time(~0)
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
