#pragma once

#include "../effect.hpp"
#include "../property.hpp"
#include "../camera.hpp"

struct Scene;
class DeferredContext;


class ParticleTest : public Effect {
public:

  ParticleTest(const std::string &name);
  ~ParticleTest();
  virtual bool init() override;
  virtual bool update(int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:

  bool file_changed(const char *filename, void *token);

  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);

  PropertyId _view_mtx_id, _view_proj_mtx_id, _proj_mtx_id;

  GraphicsObjectHandle _particle_technique;
  GraphicsObjectHandle _particle_texture;
  GraphicsObjectHandle _vb;

  Scene *_scene;

  bool _use_freefly_camera;
  FreeFlyCamera _freefly_camera;
  int _mouse_horiz;
  int _mouse_vert;
  bool _mouse_lbutton;
  bool _mouse_rbutton;
  DWORD _mouse_pos_prev;
  int _keystate[256];

  XMFLOAT4X4 _view, _proj;

  DeferredContext *_ctx;
};
