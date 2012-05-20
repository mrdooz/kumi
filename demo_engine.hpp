#pragma once

#include <queue>
#include "graphics.hpp"
#include "json_utils.hpp"
#include "property_manager.hpp"
#include "tweakable_param.hpp"

class Effect {
public:
	Effect(GraphicsObjectHandle context, const std::string &name) : _context(context), _name(name), _running(false), _start_time(~0), _end_time(~0) {}
	virtual ~Effect() {}
	virtual bool init() { return true; }
  virtual bool reset() { return true; }
	virtual bool update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) { return true; }
	virtual bool render() { return true; }
	virtual bool close() { return true; }
  const std::string name() const { return _name; }

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

	bool add_effect(Effect *effect, uint32 start_time, uint32 end_time);

	bool start();
	void set_paused(bool pause);
  bool paused() const;

  void set_pos(uint32 ms);
  uint32 duration() const;
	bool tick();

  JsonValue::JsonValuePtr get_info();
  void update(JsonValue::JsonValuePtr state);

private:
	DemoEngine();
	static DemoEngine *_instance;

  void reclassify_effects();

  Effect *find_effect_by_name(const std::string &name);

	std::deque<Effect *> _active_effects;
	std::deque<Effect *> _inactive_effects;
  std::deque<Effect *> _expired_effects;
	std::vector<Effect *> _effects;
	int _cur_effect;
	int64 _frequency;
	int64 _last_time;
  int64 _current_time;
  uint32 _duration_ms;

  PropertyManager::PropertyId _time_id;
	
	bool _paused;
};

#define DEMO_ENGINE DemoEngine::instance()
