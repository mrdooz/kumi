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
private:
  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);

  GraphicsObjectHandle _vb;
  GraphicsObjectHandle _ib;
  GraphicsObjectHandle _technique;

  GraphicsObjectHandle _normalMap;

  XMFLOAT4X4 _view, _proj;
  XMFLOAT3 _cameraPos;
  XMFLOAT3 _lightPos;
  int _numCubes;

  double _curTime;

  DeferredContext *_ctx;
};
