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

static void addCube(float x, float y, float z, float wn, float wf, float hn, float hf, float d, const XMFLOAT4X4 &mtx, PosTangentSpace2 *dst) {

  XMFLOAT3 verts[] = {
    XMFLOAT3(-wn, +hn, -d),
    XMFLOAT3(+wn, +hn, -d),
    XMFLOAT3(-wn, -hn, -d),
    XMFLOAT3(+wn, -hn, -d),
    XMFLOAT3(-wf, +hf, +d),
    XMFLOAT3(+wf, +hf, +d),
    XMFLOAT3(-wf, -hf, +d),
    XMFLOAT3(+wf, -hf, +d),
  };

  int indices[] = {
    2, 0, 1,  // front
    2, 1, 3,
    3, 1, 5,  // right
    3, 5, 7,
    7, 5, 4,  // back
    7, 4, 6,
    6, 4, 0,  // left
    6, 0, 2,
    0, 4, 5,  // top
    0, 5, 1,
    6, 2, 3,  // bottom
    6, 3, 7,
  };

  XMFLOAT2 tex[] = {
    XMFLOAT2(0,1),
    XMFLOAT2(0,0),
    XMFLOAT2(1,0),
    XMFLOAT2(0,1),
    XMFLOAT2(1,0),
    XMFLOAT2(1,1),
  };

  XMFLOAT3 tangentSpace[] = {
    // tangent, normal
    // bitangent = cross(normal, tangent)
    XMFLOAT3(-1,0,0), XMFLOAT3(0,0,-1),
    XMFLOAT3(0,0,-1), XMFLOAT3(+1,0,0),
    XMFLOAT3(+1,0,0), XMFLOAT3(0,0,+1),
    XMFLOAT3(0,0,+1), XMFLOAT3(-1,0,0),
    XMFLOAT3(-1,0,0), XMFLOAT3(0,+1,0),
    XMFLOAT3(-1,0,0), XMFLOAT3(0,-1,0),
  };

  XMFLOAT3 p(x,y,z);
  for (int i = 0; i < ELEMS_IN_ARRAY(indices); ++i) {
    int idx = indices[i];
    int tIdx = 2*(i/6);
    *dst++ = PosTangentSpace2(p + verts[idx] * mtx, 
      tex[i%6],
      tangentSpace[tIdx+0] * mtx, tangentSpace[tIdx+1]);
  }
}
/*
static int fillSpace(PosNormal *dst, float x, float y, float z, float cubeHeight, float value, int dir, const XMFLOAT4X4 &parent, int level, int maxLevel) {

  if (level >= maxLevel)
    return 0;

  //if (value < 0.1f || cubeHeight < 1)
  //return 0;

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
    mtx = mtx_from_axis_angle(axis[dir], gaussianRand(0, XM_PI));
  }

  XMStoreFloat4x4(&mtx, XMMatrixMultiply(XMLoadFloat4x4(&mtx), XMLoadFloat4x4(&parent)));

  addCube(x, y, z, cubeHeight, cubeHeight, cubeHeight, mtx, dst);

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
      res += fillSpace(dst + 36*res, pos.x, pos.y, pos.z, cubeHeight/f, value - cubeHeight, i, mtx, level + 1, maxLevel);
    }
  }

  return res;
}
*/
BoxThing::BoxThing(const std::string &name) 
  : Effect(name)
  , _ctx(nullptr)
  , _numCubes(1)
  , _curTime(0)
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

BoxThing::~BoxThing() {
  GRAPHICS.destroy_deferred_context(_ctx);
}

bool BoxThing::init() {

  int x = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx = GRAPHICS.create_deferred_context(true);

  _vb = GFX_create_buffer(D3D11_BIND_VERTEX_BUFFER, 16 * 1024 * 1024, true, nullptr, sizeof(PosTangentSpace2));

  GRAPHICS.load_techniques("effects/box_thing.tec", false);
  _technique = GRAPHICS.find_technique("box_thing");

  _freefly_camera.setPos(0, 0, -100);

  _normalMap = RESOURCE_MANAGER.load_texture("gfx/normalmap2.png", "normalmap2.png", false, nullptr);

  return true;
}

void BoxThing::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  *proj = _freefly_camera.projectionMatrix();
  if (_useFreeflyCamera) {
    _cameraPos = _freefly_camera.pos();
    *view = _freefly_camera.viewMatrix();
  } else {
  }
}

bool BoxThing::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time / 1000.0;
  _curTime = local_time / 1000.0;

  calc_camera_matrices(time, delta_ns / 1e6, &_view, &_proj);

  _lightPos.x = 100 * sinf((float)time);
  _lightPos.y = 100 * cosf((float)time);
  _lightPos.z = 100 * sinf(cosf((float)time));

  return true;
}

bool BoxThing::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  ScopedMap<PosTangentSpace2> pn(_ctx, D3D11_MAP_WRITE_DISCARD, _vb);
  int numSegments = 20;
  float step = 2 * XM_PI / numSegments;

  PosTangentSpace2 *p = pn;

  _numCubes = 0;
  int numCogs = 100;
  for (int j = 0; j < numCogs; ++j) {
    float angle = (float)j;
    for (int i = 0; i < numSegments; ++i) {
      XMFLOAT4X4 mtx = mtx_from_axis_angle(XMFLOAT3(1,0,0), angle);
      XMFLOAT3 ofs = XMFLOAT3(0, 0, -10) * mtx;
      angle += step;
      addCube(ofs.x+(j-numCogs/2)*4, ofs.y, ofs.z, 2, 1, 2, 1, 10, mtx, p);
      p += 36;
      _numCubes++;
    }

  }
/*
  float step = 1 / 150.0f;
  float t = (float)_curTime / 2;
  PosTangentSpace2 *p = pn;
  XMFLOAT3 pos(-200,0,0);
  XMFLOAT4X4 mtx(mtx_identity());
  _numCubes = 0;
  float cubeWidth = 4;
  float angle = t*2;
  for (int i = 0; i < 100; ++i) {
    mtx = mtx_from_axis_angle(XMFLOAT3(sinf(t),cosf(t),0), angle);
    addCube(pos.x, pos.y, pos.z, cubeWidth, 10, 10 + 5*sinf(i/10.0f), mtx, p);
    p += 36;
    pos.x += cubeWidth;
    pos.y = 10 + 9 * sinf(5*t);
    t += step;
    angle += XM_PI/6;
    _numCubes++;
  }
*/
  pn.unmap();

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
    XMFLOAT3 eyePos;
  } cbuffer;

  XMMATRIX xmViewProj = XMMatrixMultiply(XMLoadFloat4x4(&_view), XMLoadFloat4x4(&_proj));
  XMStoreFloat4x4(&cbuffer.worldViewProj, XMMatrixTranspose(xmViewProj));
  cbuffer.worldView = transpose(_view);
  cbuffer.lightPos = expand(_lightPos, 0);
  cbuffer.eyePos = _cameraPos;

  _ctx->set_samplers(ps->samplers());
  _ctx->set_shader_resource(_normalMap, ShaderType::kPixelShader);

  _ctx->set_cbuffer(vs->cbuffer_by_index(0), 0, ShaderType::kVertexShader, &cbuffer, sizeof(cbuffer));
  if (ps->cbuffer_count())
    _ctx->set_cbuffer(ps->cbuffer_by_index(0), 0, ShaderType::kPixelShader, &cbuffer, sizeof(cbuffer));

  _ctx->set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _ctx->set_vb(_vb);
  _ctx->draw(_numCubes * 36, 0);

  _ctx->end_frame();

  return true;
}

bool BoxThing::close() {
  return true;
}

