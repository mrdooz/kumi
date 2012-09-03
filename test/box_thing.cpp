#include "stdafx.h"
#include "box_thing.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_interface.hpp"
#include "../scene.hpp"
#include "../threading.hpp"
#include "../mesh.hpp"
#include "../tweakable_param.hpp"
#include "../graphics.hpp"
#include "../material.hpp"
#include "../material_manager.hpp"
#include "../xmath.hpp"
#include "../profiler.hpp"
#include "../deferred_context.hpp"
#include "../animation_manager.hpp"
#include "../app.hpp"
#include "../bit_utils.hpp"
#include "../dx_utils.hpp"
#include "../vertex_types.hpp"

using namespace std;
using namespace std::tr1::placeholders;

static void addCube(float x, float y, float z, float s, const XMFLOAT4X4 &mtx, PosNormal *dst) {

  XMFLOAT3 verts[] = {
    XMFLOAT3(-s, +s, -s),
    XMFLOAT3(+s, +s, -s),
    XMFLOAT3(-s, -s, -s),
    XMFLOAT3(+s, -s, -s),
    XMFLOAT3(-s, +s, +s),
    XMFLOAT3(+s, +s, +s),
    XMFLOAT3(-s, -s, +s),
    XMFLOAT3(+s, -s, +s),
  };

  int indices[] = {
    0, 1, 2,  // front
    2, 1, 3,
    3, 1, 5,  // right
    3, 5, 7,
    7, 5, 4,  // back
    7, 4, 6,
    6, 4, 0,  // left
    6, 0, 2,
    4, 5, 0,  // top
    0, 5, 1,
    6, 2, 3,  // bottom
    6, 3, 7,
  };

  XMFLOAT3 normals[] = {
    XMFLOAT3(0,0,-1),
    XMFLOAT3(+1,0,0),
    XMFLOAT3(0,0,+1),
    XMFLOAT3(-1,0,0),
    XMFLOAT3(0,+1,0),
    XMFLOAT3(0,-1,0)
  };



  XMFLOAT3 p(x,y,z);
  for (int i = 0; i < ELEMS_IN_ARRAY(indices); ++i) {
    int idx = indices[i];
    *dst++ = PosNormal(p + verts[idx] * mtx, normals[i/6] * mtx);
  }
}

BoxThing::BoxThing(const std::string &name) 
  : Effect(name)
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
  , _ctx(nullptr)
  , _useFreeFlyCamera(true)
  , _numCubes(1)
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

BoxThing::~BoxThing() {
  GRAPHICS.destroy_deferred_context(_ctx);
}

template<typename T>
class ScopedMap {
public:
  ScopedMap(DeferredContext *ctx, D3D11_MAP type, GraphicsObjectHandle obj) : _ctx(ctx), _obj(obj) {
    _ctx->map(_obj, 0, type, 0, &_res);
  }
   ~ScopedMap() {
     _ctx->unmap(_obj, 0);
   }
   operator T *() { return (T *)_res.pData; }
private:
  D3D11_MAPPED_SUBRESOURCE _res;
  DeferredContext *_ctx;
  GraphicsObjectHandle _obj;
};

int fillSpace(PosNormal *dst, float x, float y, float z, float cubeHeight, float value, int dir, const XMFLOAT4X4 &parent) {

  if (value < 0.1f || cubeHeight < 1)
    return 0;

  XMFLOAT4X4 mtx(mtx_identity());

  XMFLOAT3 axis[] = {
    XMFLOAT3(0,+1,0),
    XMFLOAT3(0,-1,0),
    XMFLOAT3(+1,0,0),
    XMFLOAT3(-1,0,0),
    XMFLOAT3(0,0,-1),
    XMFLOAT3(0,0,+1),
  };

  if (dir != -1) {
    mtx = mtx_from_axis_angle(axis[dir], gaussianRand(0, 1.5f));
  }

  XMStoreFloat4x4(&mtx, XMMatrixMultiply(XMLoadFloat4x4(&mtx), XMLoadFloat4x4(&parent)));

  addCube(x, y, z, cubeHeight, mtx, dst);

  float f = gaussianRand(2, 0.5f);
  float d = cubeHeight + cubeHeight/f;

  int skip[] = {
    1, 0, 3, 2, 5, 4
  };

  int res = 1;
  XMFLOAT3 p(x,y,z);
  for (int i = 0; i < 6; ++i) {
    if (dir == -1 || i != skip[dir]) {
      XMFLOAT3 pos = p + d * axis[i] * mtx;
      res += fillSpace(dst + 36*res, pos.x, pos.y, pos.z, cubeHeight/f, value - cubeHeight, i, mtx);
    }
  }

  return res;
}

bool BoxThing::init() {

  int x = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx = GRAPHICS.create_deferred_context(true);

  _vb = GFX_create_buffer(D3D11_BIND_VERTEX_BUFFER, 64 * 1024 * 1024, true, nullptr, sizeof(PosNormal));

  ScopedMap<PosNormal> pn(_ctx, D3D11_MAP_WRITE_DISCARD, _vb);
  _numCubes = fillSpace(pn, 0, 0, 0, 10, 30, -1, mtx_identity());

  GRAPHICS.load_techniques("effects/box_thing.tec", false);
  _technique = GRAPHICS.find_technique("box_thing");

  _freefly_camera.setPos(0, 0, -100);

  return true;
}

void BoxThing::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  *proj = _freefly_camera.projectionMatrix();

  double orgDelta = delta;
  if (is_bit_set(GetAsyncKeyState(VK_SHIFT), 15))
    delta /= 10;

  if (_useFreeFlyCamera) {
    if (_keystate['W'])
      _freefly_camera.move(FreeFlyCamera::kForward, (float)(100 * delta));
    if (_keystate['S'])
      _freefly_camera.move(FreeFlyCamera::kForward, (float)(-100 * delta));
    if (_keystate['A'])
      _freefly_camera.move(FreeFlyCamera::kRight, (float)(-100 * delta));
    if (_keystate['D'])
      _freefly_camera.move(FreeFlyCamera::kRight, (float)(100 * delta));
    if (_keystate['Q'])
      _freefly_camera.move(FreeFlyCamera::kUp, (float)(100 * delta));
    if (_keystate['E'])
      _freefly_camera.move(FreeFlyCamera::kUp, (float)(-100 * delta));

    if (_mouse_lbutton) {
      float dx = (float)(100 * orgDelta) * _mouse_horiz / 200.0f;
      float dy = (float)(100 * orgDelta) * _mouse_vert / 200.0f;
      _freefly_camera.rotate(FreeFlyCamera::kXAxis, dx);
      _freefly_camera.rotate(FreeFlyCamera::kYAxis, dy);
    }
    _cameraPos = _freefly_camera.pos();

    *view = _freefly_camera.viewMatrix();
  } else {

  }
}

bool BoxThing::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time  / 1000.0;

  calc_camera_matrices(time, delta_ns / 1e6, &_view, &_proj);

  _lightPos.x = 100 * sinf((float)time);
  _lightPos.y = 100 * cosf((float)time);
  _lightPos.z = 100 * sinf(cosf((float)time));

  return true;
}

bool BoxThing::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx->set_default_render_target(true);

  Technique *technique;
  Shader *vs, *ps;
  setupTechnique(_ctx, _technique, false, &technique, &vs, nullptr, &ps);

  struct {
    XMFLOAT4X4 worldViewProj;
    XMFLOAT4X4 worldView;
    XMFLOAT4 lightPos;
    XMFLOAT4 eyePos;
  } cbuffer;

  XMMATRIX xmViewProj = XMMatrixMultiply(XMLoadFloat4x4(&_view), XMLoadFloat4x4(&_proj));
  XMStoreFloat4x4(&cbuffer.worldViewProj, XMMatrixTranspose(xmViewProj));
  cbuffer.worldView = transpose(_view);
  cbuffer.lightPos = expand(_lightPos, 0);
  cbuffer.eyePos = expand(_cameraPos, 0);

  _ctx->set_cbuffer(vs->find_cbuffer("cbMain"), 0, ShaderType::kVertexShader, &cbuffer, sizeof(cbuffer));
  _ctx->set_cbuffer(ps->find_cbuffer("cbMain"), 0, ShaderType::kPixelShader, &cbuffer, sizeof(cbuffer));

  _ctx->set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _ctx->set_vb(_vb);
  //_ctx->set_ib(_ib, DXGI_FORMAT_R32_UINT);
  _ctx->draw(_numCubes * 36, 0);

  _ctx->end_frame();

  return true;
}

bool BoxThing::close() {
  return true;
}

void BoxThing::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
  case WM_KEYDOWN:
    if (wParam <= 255)
      _keystate[wParam] = 1;
    break;

  case WM_KEYUP:
    if (wParam <= 255)
      _keystate[wParam] = 0;
    switch (wParam) {
    case 'F':
      _useFreeFlyCamera = !_useFreeFlyCamera;
      break;
    case 'Z':
      //_useZFill = !_useZFill;
      break;
    }
    break;

  case WM_MOUSEMOVE:
    if (_mouse_pos_prev != ~0) {
      _mouse_horiz = LOWORD(lParam) - LOWORD(_mouse_pos_prev);
      _mouse_vert = HIWORD(lParam) - HIWORD(_mouse_pos_prev);
    }
    _mouse_pos_prev = lParam;
    break;

  case WM_LBUTTONDOWN:
    _mouse_lbutton = true;
    break;

  case WM_LBUTTONUP:
    _mouse_lbutton = false;
    break;

  case WM_RBUTTONDOWN:
    _mouse_rbutton = true;
    break;

  case WM_RBUTTONUP:
    _mouse_rbutton = false;
    break;
  }

}
