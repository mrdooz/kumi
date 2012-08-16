#pragma once

#include "json_utils.hpp"
#include "property_manager.hpp"

class TweakableParameter {
public:
  enum Type {
    kTypeFloat,
  };
  TweakableParameter(const std::string &name, float value);
  TweakableParameter(const std::string &name, float value, float minValue, float maxValue);

  float floatValue() const { KASSERT(_type == kTypeFloat); return _value._float; }
  float floatMin() const { KASSERT(_type == kTypeFloat); return _minValue._float; }
  float floatMax() const { KASSERT(_type == kTypeFloat); return _maxValue._float; }
  const std::string &name() const { return _name; }

  bool isBounded() const { return _bounded; }
  Type type() const { return _type; }

private:
  union Value {
    float _float4[4];
    float _float;
    int _int;
  };

  Value _value;
  Value _minValue;
  Value _maxValue;
  Type _type;
  std::string _name;
  bool _bounded;
};

struct TweakableParameterBlock {
  TweakableParameterBlock(const std::string &name) : _blockName(name) {}
  std::string _blockName;
  std::vector<TweakableParameter> _params;
};


class AnimatedTweakableParam {
public:
  enum Type {
    kTypeUnknown,
    kTypeCollection,  // is this silly just adding a collection type?
    kTypeBool,
    kTypeInt,
    kTypeFloat,
    kTypeFloat2,
    kTypeFloat3,
    kTypeFloat4,
    kTypeColor,
    kTypeString,
    kTypeFile,
  };

  enum Animation {
    kAnimUnknown,
    kAnimStatic,
    kAnimStep,
    kAnimLinear,
    kAnimSpline,
  };

  AnimatedTweakableParam(const std::string &name);
  AnimatedTweakableParam(const std::string &name, Type type, Animation animation);
  AnimatedTweakableParam(const std::string &name, Type type, PropertyId id);
  ~AnimatedTweakableParam();

  // TODO: handle strings..
  template<typename T>
  void add_key(uint32 time, const T &value) {
    auto v = get_values(Int2Type<T>());
    KASSERT(_animation != kAnimStatic || v->empty());
    v->push_back(Key<T>(time, value));
    // sort keys by time (if needed)
    if (v->size() != 1 && time != (*v)[v->size() - 2].time)
      std::sort(begin(*v), end(*v), [&](const Key<T> &a, const Key<T> &b) { return a.time < b.time; });
  }

  // note, the parent takes ownership of the child
  void add_child(AnimatedTweakableParam *child);

  JsonValue::JsonValuePtr to_json();

private:

  template<typename T>
  struct Key {
    Key() {}
    Key(uint32 time, T value) : time(time), value(value) {}
    uint32 time;
    T value;
  };

  JsonValue::JsonValuePtr get_keys(const AnimatedTweakableParam *p);
  JsonValue::JsonValuePtr add_param(const AnimatedTweakableParam *param);

  template<typename T> struct Int2Type {};
  std::vector<Key<bool> > *&get_values(Int2Type<bool>) { KASSERT(_type == kTypeBool); return _bool; }
  std::vector<Key<int> > *&get_values(Int2Type<int>) { KASSERT(_type == kTypeInt); return _int; }
  std::vector<Key<float> > *&get_values(Int2Type<float>) { KASSERT(_type == kTypeFloat); return _float; }
  std::vector<Key<XMFLOAT2> > *&get_values(Int2Type<XMFLOAT2>) { KASSERT(_type == kTypeFloat2); return _float2; }
  std::vector<Key<XMFLOAT3> > *&get_values(Int2Type<XMFLOAT3>) { KASSERT(_type == kTypeFloat3); return _float3; }
  std::vector<Key<XMFLOAT4> > *&get_values(Int2Type<XMFLOAT4>) { KASSERT(_type == kTypeFloat4 || _type == kTypeColor); return _float4; }
  std::string *&get_values(Int2Type<std::string>) { KASSERT(_type == kTypeString || _type == kTypeFile); return _string; }

  PropertyId _id;

  std::string _name;
  Type _type;
  Animation _animation;

  union {
    std::vector<Key<bool> > *_bool;
    std::vector<Key<int> > *_int;
    std::vector<Key<float> > *_float;
    std::vector<Key<XMFLOAT2> > *_float2;
    std::vector<Key<XMFLOAT3> > *_float3;
    std::vector<Key<XMFLOAT4> > *_float4;
    std::string *_string;
  };

  std::vector<AnimatedTweakableParam *> _children;
};
