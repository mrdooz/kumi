#pragma once

#include "../effect.hpp"
#include "../property.hpp"
#include "../camera.hpp"
#include "../gaussian_blur.hpp"

struct Scene;
class DeferredContext;

class BoxThing : public Effect {
public:
  BoxThing(const std::string &name);
  ~BoxThing();
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:

  GraphicsObjectHandle _vb;
  GraphicsObjectHandle _ib;
  GraphicsObjectHandle _technique;

  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);

  bool _useFreeFlyCamera;
  FreeFlyCamera _freefly_camera;
  int _mouse_horiz;
  int _mouse_vert;
  bool _mouse_lbutton;
  bool _mouse_rbutton;
  DWORD _mouse_pos_prev;
  int _keystate[256];

  XMFLOAT4X4 _view, _proj;
  XMFLOAT3 _cameraPos;
  XMFLOAT3 _lightPos;
  int _numCubes;

  DeferredContext *_ctx;
};
