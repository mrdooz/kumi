#pragma once

#include "graphics_submit.hpp"
#include "json_utils.hpp"

class AnimatedTweakableParam;

class Effect {
public:
  Effect(const std::string &name);
  virtual ~Effect();
  virtual bool init();
  virtual bool reset();
  virtual bool update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction);
  virtual bool render();
  virtual bool close();

  JsonValue::JsonValuePtr get_info() const;
  virtual void update_from_json(const JsonValue::JsonValuePtr &state);

  const std::string &name() const { return _name; }
  int64 start_time() const { return _start_time; }
  int64 end_time() const { return _end_time; }
  bool running() const { return _running; }
  void set_running(bool b) { _running = b; }
  void set_duration(int64 start_time, int64 end_time);
  void set_start_time(int64 startTime);

  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {}
  virtual void fill_cbuffer(CBuffer *cbuffer) const {}

protected:
  std::string _name;
  int64 _start_time, _end_time;
  bool _running;
  std::vector<std::unique_ptr<AnimatedTweakableParam> > _params;
};

