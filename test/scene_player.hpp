#pragma once

#include "../effect.hpp"
#include "../property_manager.hpp"

struct Scene;

struct FreeFlyCamera {
  FreeFlyCamera() : theta(XM_PIDIV2), rho(XM_PIDIV2), roll(0), pos(0,0,0), dir(0,0,1), right(1,0,0), up(0,1,0) {} // points down the z-axis
  float theta;
  float rho;
  float roll;
  XMFLOAT3 pos;
  XMFLOAT3 dir, right, up;
};

class ScenePlayer : public Effect {
public:

  ScenePlayer(GraphicsObjectHandle context, const std::string &name);
  ~ScenePlayer();
  virtual bool init() override;
  virtual bool update(int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;

  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:

  virtual void update_from_json(const JsonValue::JsonValuePtr &state) override;
  bool file_changed(const char *filename, void *token);

  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);

  PropertyId _light_pos_id, _light_color_id, _light_att_start_id, _light_att_end_id;
  PropertyId _view_mtx_id, _proj_mtx_id;

  Scene *_scene;

  bool _use_freefly_camera;
  FreeFlyCamera _freefly_camera;
  int _mouse_horiz;
  int _mouse_vert;
  bool _mouse_lbutton;
  bool _mouse_rbutton;
  DWORD _mouse_pos_prev;
  int _keystate[256];

  GraphicsObjectHandle _default_shader;
  GraphicsObjectHandle _ssao_fill;
  GraphicsObjectHandle _ssao_compute;
  GraphicsObjectHandle _ssao_blur;
  GraphicsObjectHandle _ssao_ambient;
  GraphicsObjectHandle _ssao_light;
  GraphicsObjectHandle _gamma_correct;

  GraphicsObjectHandle _luminance_map;

  PropertyId _kernel_id, _noise_id, _ambient_id;

  XMFLOAT4X4 _view, _proj;
};
