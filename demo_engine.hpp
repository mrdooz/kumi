#pragma once

#include <queue>
#include "graphics.hpp"
#include "json_utils.hpp"

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
    // sort keys by time if needed
    if (v->size() != 1 && time != (*v)[v->size() - 2].time)
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
	Effect(GraphicsObjectHandle context, const std::string &name) : _context(context), _name(name), _running(false), _start_time(~0), _end_time(~0) {}
	virtual ~Effect() {}
	virtual bool init() { return true; }
	virtual bool update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) { return true; }
	virtual bool render() { return true; }
	virtual bool close() { return true; }
  const std::string name() const { return _name; }
  //const std::vector<TweakableParam> &params() const { return _params; }

  JsonValue::JsonValuePtr get_info() const;

  uint32 start_time() const { return _start_time; }
  uint32 end_time() const { return _end_time; }
  bool running() const { return _running; }
  void set_running(bool b) { _running = b; }
  void set_duration(uint32 start_time, uint32 end_time) { _start_time = start_time; _end_time = end_time; }
protected:
	std::string _name;
	GraphicsObjectHandle _context;
  uint32 _start_time, _end_time;
  bool _running;
  std::vector<std::unique_ptr<TweakableParam> > _params;
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

  JsonValue::JsonValuePtr get_info();

  void reset_current_effect();

private:
	DemoEngine();
	static DemoEngine *_instance;

	std::deque<Effect *> _active_effects;
	std::deque<Effect *> _inactive_effects;
	std::vector<Effect *> _effects;
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
