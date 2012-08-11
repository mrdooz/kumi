#include "stdafx.h"
#include "tweakable_param.hpp"
#include "utils.hpp"

using namespace std;

TweakableParameter::TweakableParameter(const std::string &name, float value) 
  : _name(name)
  , _type(kTypeFloat)
  , _bounded(false)
{
  _value._float = value;
}

TweakableParameter::TweakableParameter(const std::string &name, float value, float minValue, float maxValue) 
  : _name(name)
  , _type(kTypeFloat)
  , _bounded(true)
{
  _value._float = value;
  _minValue._float = minValue;
  _maxValue._float = maxValue;
}


static string type_to_string(AnimatedTweakableParam::Type type) {
  switch (type) {
  case AnimatedTweakableParam::kTypeUnknown: return "unknown";
  case AnimatedTweakableParam::kTypeBool: return "bool";
  case AnimatedTweakableParam::kTypeInt: return "int";
  case AnimatedTweakableParam::kTypeFloat: return "float";
  case AnimatedTweakableParam::kTypeFloat2: return "float2";
  case AnimatedTweakableParam::kTypeFloat3: return "float3";
  case AnimatedTweakableParam::kTypeFloat4: return "float4";
  case AnimatedTweakableParam::kTypeColor: return "color";
  case AnimatedTweakableParam::kTypeString: return "string";
  case AnimatedTweakableParam::kTypeFile: return "file";
  }
  return "";
}

static string anim_to_string(AnimatedTweakableParam::Animation anim) {
  switch (anim) {
  case AnimatedTweakableParam::kAnimUnknown: return "unknown";
  case AnimatedTweakableParam::kAnimStatic: return "static";
  case AnimatedTweakableParam::kAnimStep: return "step";
  case AnimatedTweakableParam::kAnimLinear: return "linear";
  case AnimatedTweakableParam::kAnimSpline: return "spline";
  }
  return "";
}

AnimatedTweakableParam::AnimatedTweakableParam(const std::string &name, Type type, Animation animation) 
  : _name(name)
  , _type(type)
  , _animation(animation)
  , _id(~0) {
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

AnimatedTweakableParam::AnimatedTweakableParam(const std::string &name, Type type, PropertyId id)
  : _name(name)
  , _type(type)
  , _animation(kAnimStatic)
  , _id(id) {
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

AnimatedTweakableParam::AnimatedTweakableParam(const std::string &name)
  : _name(name)
  , _type(kTypeUnknown)
  , _animation(kAnimUnknown)
  , _id(~0) {
}

AnimatedTweakableParam::~AnimatedTweakableParam() {
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

  seq_delete(&_children);
}

void AnimatedTweakableParam::add_child(AnimatedTweakableParam *child) {
  _children.push_back(child);
}

JsonValue::JsonValuePtr AnimatedTweakableParam::to_json() {

  auto obj = JsonValue::create_object();
  obj->add_key_value("name", _name);
  if (_id != ~0)
    obj->add_key_value("id", _id);

  if (_type != kTypeUnknown) {
    obj->add_key_value("type", type_to_string(_type));
    obj->add_key_value("anim", anim_to_string(_animation));
    obj->add_key_value("keys", get_keys(this));
  }

  if (!_children.empty()) {
    auto children = JsonValue::create_array();
    for (auto it = begin(_children); it != end(_children); ++it) {
      children->add_value((*it)->to_json());
    }

    obj->add_key_value("children", children);
  }

  return obj;
}



JsonValue::JsonValuePtr AnimatedTweakableParam::get_keys(const AnimatedTweakableParam *p) {
  auto a = JsonValue::create_array();
  auto type = p->_type;

  switch (type) {

    case AnimatedTweakableParam::kTypeBool: {
      for (auto it = begin(*p->_bool); it != end(*p->_bool); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        o->add_key_value("value", it->value);
        a->add_value(o);
      }
      break;
    }

    case AnimatedTweakableParam::kTypeInt: {
      for (auto it = begin(*p->_int); it != end(*p->_int); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        o->add_key_value("value", it->value);
        a->add_value(o);
      }
      break;
    }

    case AnimatedTweakableParam::kTypeFloat: {
      for (auto it = begin(*p->_float); it != end(*p->_float); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        auto v = JsonValue::create_object();
        v->add_key_value("x", it->value);
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case AnimatedTweakableParam::kTypeFloat2: {
      for (auto it = begin(*p->_float2); it != end(*p->_float2); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        auto v = JsonValue::create_object();
        v->add_key_value("x", it->value.x);
        v->add_key_value("y", it->value.y);
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case AnimatedTweakableParam::kTypeFloat3: {
      for (auto it = begin(*p->_float3); it != end(*p->_float3); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        auto v = JsonValue::create_object();
        v->add_key_value("x", it->value.x);
        v->add_key_value("y", it->value.y);
        v->add_key_value("z", it->value.z);
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case AnimatedTweakableParam::kTypeColor:
    case AnimatedTweakableParam::kTypeFloat4: {
      for (auto it = begin(*p->_float4); it != end(*p->_float4); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        auto v = JsonValue::create_object();
        v->add_key_value(type == AnimatedTweakableParam::kTypeColor ? "r" : "x", it->value.x);
        v->add_key_value(type == AnimatedTweakableParam::kTypeColor ? "g" : "y", it->value.y);
        v->add_key_value(type == AnimatedTweakableParam::kTypeColor ? "b" : "z", it->value.z);
        v->add_key_value(type == AnimatedTweakableParam::kTypeColor ? "a" : "w", it->value.w);
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case AnimatedTweakableParam::kTypeString:
    case AnimatedTweakableParam::kTypeFile: {
      // these guys can only have a single key
      auto o = JsonValue::create_object();
      o->add_key_value("time", (int)0);
      o->add_key_value("value", *p->_string);
      a->add_value(o);
      break;
    }
  }

  return a;
}

JsonValue::JsonValuePtr AnimatedTweakableParam::add_param(const AnimatedTweakableParam *param) {
  auto obj = JsonValue::create_object();
  obj->add_key_value("name", JsonValue::create_string(param->_name));
  obj->add_key_value("type", JsonValue::create_string(type_to_string(param->_type)));
  obj->add_key_value("anim", JsonValue::create_string(anim_to_string(param->_animation)));
  obj->add_key_value("keys", get_keys(param));

  return obj;
}
