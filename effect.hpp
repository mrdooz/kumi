#pragma once

#include "json_utils.hpp"
#include "camera.hpp"

class AnimatedTweakableParam;
struct CBuffer;

class Effect {
public:
  Effect(const std::string &name);
  virtual ~Effect();
  virtual bool init();
  virtual bool reset();
  virtual bool render();
  virtual bool close();

  bool updateBase(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction);
  void wndProcBase(UINT message, WPARAM wParam, LPARAM lParam);

  JsonValue::JsonValuePtr get_info() const;
  virtual void update_from_json(const JsonValue::JsonValuePtr &state);

  virtual void fill_cbuffer(CBuffer *cbuffer) const {}

  const std::string &name() const { return _name; }
  int64 start_time() const { return _start_time; }
  int64 end_time() const { return _end_time; }
  bool running() const { return _running; }
  void set_running(bool b) { _running = b; }
  void set_duration(int64 start_time, int64 end_time);
  void set_start_time(int64 startTime);

protected:

  virtual bool update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction);
  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {}

  void updateFreeflyCamera(double time, double delta);

  bool _useFreeflyCamera;
  FreeflyCamera _freefly_camera;
  int _mouse_horiz;
  int _mouse_vert;
  bool _mouse_lbutton;
  bool _mouse_rbutton;
  DWORD _mouse_pos_prev;
  int _keystate[256];

  std::string _name;
  int64 _start_time, _end_time;
  bool _running;
  std::vector<std::unique_ptr<AnimatedTweakableParam> > _params;
};

