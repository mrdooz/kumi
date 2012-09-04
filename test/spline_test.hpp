#pragma once

#include "../effect.hpp"
#include "../property.hpp"
#include "../camera.hpp"
#include "../gaussian_blur.hpp"

struct Scene;
class DeferredContext;
class DynamicSpline;

class DynamicSpline;

class SplineTest : public Effect {
public:
  SplineTest(const std::string &name);
  ~SplineTest();
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
private:

  void renderSplines(GraphicsObjectHandle rtMirror);
  void renderPlane(GraphicsObjectHandle rtMirror);
  void renderParticles();

  void post_process(GraphicsObjectHandle input, GraphicsObjectHandle output, GraphicsObjectHandle technique);

  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);

  bool initSplines();

  void createSplineCallback(const XMFLOAT3 &p, const XMFLOAT3 &dir, const XMFLOAT3 &n, DynamicSpline *parent);
  
  GraphicsObjectHandle _particle_technique;
  GraphicsObjectHandle _particle_texture;
  GraphicsObjectHandle _particleVb;

  GraphicsObjectHandle _planeVb;
  GraphicsObjectHandle _planeIb;

  GraphicsObjectHandle _gradient_technique;
  GraphicsObjectHandle _compose_technique;
  GraphicsObjectHandle _coalesce;

  GraphicsObjectHandle _scale;

  GraphicsObjectHandle _splineTechnique;
  GraphicsObjectHandle _planeTechnique;

  GraphicsObjectHandle _cubeMap;
  
  GaussianBlur _blur;

  float _blurX, _blurY;

  float _nearFocusStart, _nearFocusEnd;
  float _farFocusStart, _farFocusEnd;

  bool _useZFill;

  GraphicsObjectHandle _dynamicVb;
  GraphicsObjectHandle _staticVb;
  int _staticVertCount;

  XMFLOAT4X4 _view, _proj;
  XMFLOAT3 _cameraPos;

  DeferredContext *_ctx;
  std::vector<XMFLOAT4> _extrudeShape;

  std::vector<DynamicSpline *> _splines;
};
