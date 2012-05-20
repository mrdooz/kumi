#include "stdafx.h"
#include "tweakable_param.hpp"
#include "utils.hpp"

using namespace std;

static string type_to_string(TweakableParam::Type type) {
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

static string anim_to_string(TweakableParam::Animation anim) {
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
        o->add_key_value("time", it->time);
        o->add_key_value("value", it->value);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeInt: {
      for (auto it = begin(*p->_int); it != end(*p->_int); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        o->add_key_value("value", it->value);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeFloat: {
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

    case TweakableParam::kTypeFloat2: {
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

    case TweakableParam::kTypeFloat3: {
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

    case TweakableParam::kTypeColor:
    case TweakableParam::kTypeFloat4: {
      for (auto it = begin(*p->_float4); it != end(*p->_float4); ++it) {
        auto o = JsonValue::create_object();
        o->add_key_value("time", it->time);
        auto v = JsonValue::create_object();
        v->add_key_value(type == TweakableParam::kTypeColor ? "r" : "x", it->value.x);
        v->add_key_value(type == TweakableParam::kTypeColor ? "g" : "y", it->value.y);
        v->add_key_value(type == TweakableParam::kTypeColor ? "b" : "z", it->value.z);
        v->add_key_value(type == TweakableParam::kTypeColor ? "a" : "w", it->value.w);
        o->add_key_value("value", v);
        a->add_value(o);
      }
      break;
    }

    case TweakableParam::kTypeString:
    case TweakableParam::kTypeFile: {
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

JsonValue::JsonValuePtr add_param(const TweakableParam *param) {
  auto obj = JsonValue::create_object();
  obj->add_key_value("name", JsonValue::create_string(param->_name));
  obj->add_key_value("type", JsonValue::create_string(type_to_string(param->_type)));
  obj->add_key_value("anim", JsonValue::create_string(anim_to_string(param->_animation)));
  obj->add_key_value("keys", get_keys(param));

  return obj;
}


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

JsonValue::JsonValuePtr TweakableParam::to_json() {

  auto obj = JsonValue::create_object();
  obj->add_key_value("name", _name);
  obj->add_key_value("type", type_to_string(_type));
  obj->add_key_value("anim", anim_to_string(_animation));
  obj->add_key_value("keys", get_keys(this));

  return obj;
}
