#pragma once

#include "json_utils.hpp"

class TweakableParam {
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

  TweakableParam(const std::string &name);
  TweakableParam(const std::string &name, Type type, Animation animation);
  ~TweakableParam();

  // TODO: handle strings..
  template<typename T>
  void add_key(uint32 time, const T &value) {
    auto v = get_values(Int2Type<T>());
    assert(_animation != kAnimStatic || v->empty());
    v->push_back(Key<T>(time, value));
    // sort keys by time (if needed)
    if (v->size() != 1 && time != (*v)[v->size() - 2].time)
      std::sort(begin(*v), end(*v), [&](const Key<T> &a, const Key<T> &b) { return a.time < b.time; });
  }

  // note, the parent takes ownership of the child
  void add_child(TweakableParam *child);

  JsonValue::JsonValuePtr to_json();

private:

  template<typename T>
  struct Key {
    Key() {}
    Key(uint32 time, T value) : time(time), value(value) {}
    uint32 time;
    T value;
  };

  JsonValue::JsonValuePtr get_keys(const TweakableParam *p);
  JsonValue::JsonValuePtr add_param(const TweakableParam *param);

  template<typename T> struct Int2Type {};
  std::vector<Key<bool> > *&get_values(Int2Type<bool>) { assert(_type == kTypeBool); return _bool; }
  std::vector<Key<int> > *&get_values(Int2Type<int>) { assert(_type == kTypeInt); return _int; }
  std::vector<Key<float> > *&get_values(Int2Type<float>) { assert(_type == kTypeFloat); return _float; }
  std::vector<Key<XMFLOAT2> > *&get_values(Int2Type<XMFLOAT2>) { assert(_type == kTypeFloat2); return _float2; }
  std::vector<Key<XMFLOAT3> > *&get_values(Int2Type<XMFLOAT3>) { assert(_type == kTypeFloat3); return _float3; }
  std::vector<Key<XMFLOAT4> > *&get_values(Int2Type<XMFLOAT4>) { assert(_type == kTypeFloat4 || _type == kTypeColor); return _float4; }
  std::string *&get_values(Int2Type<std::string>) { assert(_type == kTypeString || _type == kTypeFile); return _string; }

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

  std::vector<TweakableParam *> _children;
};
