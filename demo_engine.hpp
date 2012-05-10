#pragma once

#include <queue>
#include "graphics.hpp"

struct TweakableParam {
  enum Type {
    kTypeUnknown,
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

  template<typename T>
  struct Key {
    Key() {}
    Key(uint32 time, T value) : time(time), value(value) {}
    uint32 time;
    T value;
  };

  TweakableParam();
  TweakableParam(Type type, Animation animation);
  ~TweakableParam();

  template<typename T> struct Int2Type {};
  vector<Key<bool> > *&get_values(Int2Type<bool>) { assert(_type == kTypeBool); return _bool; }
  vector<Key<int> > *&get_values(Int2Type<int>) { assert(_type == kTypeInt); return _int; }
  vector<Key<float> > *&get_values(Int2Type<float>) { assert(_type == kTypeFloat); return _float; }
  vector<Key<XMFLOAT2> > *&get_values(Int2Type<XMFLOAT2>) { assert(_type == kTypeFloat2); return _float2; }
  vector<Key<XMFLOAT3> > *&get_values(Int2Type<XMFLOAT3>) { assert(_type == kTypeFloat3); return _float3; }
  vector<Key<XMFLOAT4> > *&get_values(Int2Type<XMFLOAT4>) { assert(_type == kTypeFloat4 || _type == kTypeColor); return _float4; }
  std::string *&get_values(Int2Type<std::string>) { assert(_type == kTypeString || _type == kTypeFile); return _string; }

  // TODO: handle strings..
  template<typename T>
  void add_key(uint32 time, const T &value) {
    auto v = get_values(Int2Type<T>());
    assert(_animation != kAnimStatic || v->empty());
    v->push_back(Key<T>(time, value));
    // sort keys by time
    std::sort(begin(*v), end(*v), [&](const Key<T> &a, const Key<T> &b) { return a.time < b.time; });
  }

  std::string _name;
  Type _type;
  Animation _animation;

  union {
    vector<Key<bool> > *_bool;
    vector<Key<int> > *_int;
    vector<Key<float> > *_float;
    vector<Key<XMFLOAT2> > *_float2;
    vector<Key<XMFLOAT3> > *_float3;
    vector<Key<XMFLOAT4> > *_float4;
    std::string *_string;
  };
};

class Effect {
public:
	Effect(GraphicsObjectHandle context, const std::string &name) : _context(context), _name(name) {}
	virtual ~Effect() {}
	virtual bool init() { return true; }
	virtual bool update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) { return true; }
	virtual bool render() { return true; }
	virtual bool close() { return true; }
  const std::string name() const { return _name; }
  const std::vector<TweakableParam> &params() const { return _params; }

  std::string get_info() const;
protected:
	std::string _name;
	GraphicsObjectHandle _context;
  std::vector<TweakableParam> _params;
};

class DemoEngine {
public:
	static DemoEngine &instance();
  static bool create();
  static bool close();

	void add_effect(Effect *effect, uint32 start_time, uint32 end_time);
  bool init();

	bool start();
	void set_paused(bool pause);
  bool paused() const;

  void set_pos(uint32 ms);
  uint32 duration() const;
	bool tick();

  std::string get_info();

  void reset_current_effect();

private:

  // TODO: move this to effect
	struct EffectInstance {
		struct compare {
			bool operator()(const EffectInstance &a, const EffectInstance &b) {
				return a._start_time < b._start_time;
			}
		};
		EffectInstance(Effect *effect, uint32 start_time, uint32 end_time) : _effect(effect), _running(false), _start_time(start_time), _end_time(end_time) {}
		Effect *_effect;
		bool _running;
		uint32 _start_time;
		uint32 _end_time;
	};

	DemoEngine();
	static DemoEngine *_instance;

	std::deque<EffectInstance *> _active_effects;
	std::deque<EffectInstance *> _inactive_effects;
	std::vector<EffectInstance *> _effects;
	int _cur_effect;
	int64 _frequency;
	int64 _last_time;
  int64 _current_time;
  uint32 _duration_ms;
	//int64 _current_time_ms;
	
	bool _paused;
	bool _inited;
};

#define DEMO_ENGINE DemoEngine::instance()
