#include "stdafx.h"
#include "effect.hpp"
#include "tweakable_param.hpp"

Effect::Effect(GraphicsObjectHandle context, const std::string &name) 
  : _context(context), _name(name), _running(false), _start_time(~0), _end_time(~0)
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

bool Effect::update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) { 
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
  info->add_key_value("start_time", _start_time);
  info->add_key_value("end_time", _end_time);

  auto params = JsonValue::create_array();
  for (auto it = begin(_params); it != end(_params); ++it) {
    params->add_value((*it)->to_json());
  }
  info->add_key_value("params", params);
  return info;
}

void Effect::update_from_json(const JsonValue::JsonValuePtr &state) {

  _start_time = (*state)["start_time"]->get_int();
  _end_time = (*state)["end_time"]->get_int();
}

void Effect::set_duration(uint32 start_time, uint32 end_time) {
  _start_time = start_time;
  _end_time = end_time;
}
