#pragma once

#include "graphics_submit.hpp"
#include "json_utils.hpp"

class TweakableParam;

class Effect {
public:
  Effect(GraphicsObjectHandle context, const std::string &name);
  virtual ~Effect();
  virtual bool init();
  virtual bool reset();
  virtual bool update(int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction);
  virtual bool render();
  virtual bool close();

  JsonValue::JsonValuePtr get_info() const;
  virtual void update_from_json(const JsonValue::JsonValuePtr &state);

  const std::string &name() const { return _name; }
  uint32 start_time() const { return _start_time; }
  uint32 end_time() const { return _end_time; }
  bool running() const { return _running; }
  void set_running(bool b) { _running = b; }
  void set_duration(uint32 start_time, uint32 end_time);

  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {}
  virtual void fill_cbuffer(CBuffer *cbuffer) const {}

protected:
  std::string _name;
  GraphicsObjectHandle _context;
  uint32 _start_time, _end_time;
  bool _running;
  std::vector<std::unique_ptr<TweakableParam> > _params;
};

