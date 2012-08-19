#pragma once

#include "../effect.hpp"
#include "../property.hpp"
#include "../camera.hpp"
#include "../gaussian_blur.hpp"

struct Scene;
class DeferredContext;

class ParticleTest : public Effect {
public:

  ParticleTest(const std::string &name);
  ~ParticleTest();
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:

  void renderParticles();

  void post_process(GraphicsObjectHandle input, GraphicsObjectHandle output, GraphicsObjectHandle technique);

  bool file_changed(const char *filename, void *token);
  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);


  PropertyId _view_mtx_id, _view_proj_mtx_id, _proj_mtx_id;

  GraphicsObjectHandle _particle_technique;
  GraphicsObjectHandle _particle_texture;
  GraphicsObjectHandle _vb;

  GraphicsObjectHandle _gradient_technique;
  GraphicsObjectHandle _compose_technique;
  GraphicsObjectHandle _coalesce;

  GraphicsObjectHandle _scale;
  
  GaussianBlur _blur;

  float _blurX, _blurY;

  PropertyId _DofSettingsId;
  float _nearFocusStart, _nearFocusEnd;
  float _farFocusStart, _farFocusEnd;


  struct ParticleData {
    ParticleData(int numParticles);
    ~ParticleData();
    void update(float delta);
    inline void initParticle(int i);
    int numParticles;
    float *pos;
    float *posX;
    float *posY;
    float *posZ;
    float *vel;
    float *velX;
    float *velY;
    float *velZ;
    float *radius;
    float *age;
    float *maxAge;
    float *factor;
  } _particle_data;

  bool _useFreeFlyCamera;
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
