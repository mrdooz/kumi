#pragma once

#include "graphics.hpp"
#include "json_utils.hpp"
#include "property_manager.hpp"
#include "tweakable_param.hpp"

class Effect;

class DemoEngine {
public:
  static DemoEngine &instance();
  static bool create();
  static bool close();

  bool add_effect(Effect *effect, uint32 start_time, uint32 end_time);

  bool start();
  void set_paused(bool pause);
  bool paused() const;

  void set_pos(int64 ms);
  int64 pos();

  int64 duration() const; // in ms
  bool tick();

  JsonValue::JsonValuePtr get_info();
  void update(const JsonValue::JsonValuePtr &state);

  void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam);

private:
  DemoEngine();
  ~DemoEngine();
  static DemoEngine *_instance;

  void reclassify_effects();

  Effect *find_effect_by_name(const std::string &name);

  int64 ctr_to_ns(int64 ctr);

  std::deque<Effect *> _active_effects;
  std::deque<Effect *> _inactive_effects;
  std::deque<Effect *> _expired_effects;
  std::vector<Effect *> _effects;
  int _cur_effect;
  int64 _frequency;
  int64 _last_time_ctr;
  int64 _active_time_ctr;
  int64 _running_time_ctr;
  int64 _duration_ns;

  PropertyId _time_id;

  bool _paused;
};

#define DEMO_ENGINE DemoEngine::instance()
