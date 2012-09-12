#include "stdafx.h"
#include "grid_thing.hpp"
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

const int cHorizCells = 100;
const int cVertCells = 100;

static int toIdx(int x, int y) {
  return y * (cHorizCells+1)+x;
}

GridThing::GridThing(const std::string &name) 
  : Effect(name)
  , _ctx(nullptr)
  , _curTime(0)
  , _grid(cVertCells+1, cHorizCells+1, false)
  , _forces(cVertCells+1, cHorizCells+1, true)
  , _particles(cVertCells+1, cHorizCells+1, false)
  , _stiffness(10)
  , _dampening(0.99f)
  , _force(2)
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

GridThing::~GridThing() {
  GRAPHICS.destroy_deferred_context(_ctx);
}

bool GridThing::init() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx = GRAPHICS.create_deferred_context(true);
  _texture = RESOURCE_MANAGER.load_texture("gfx/background.png", "background.png", false, nullptr);

  _vb = GFX_create_buffer(D3D11_BIND_VERTEX_BUFFER, (cHorizCells+1)*(cVertCells+1)*sizeof(Pos4Tex), true, nullptr, sizeof(Pos4Tex));

  initGrid();

  GRAPHICS.load_techniques("effects/grid_thing.tec", false);
  _technique = GRAPHICS.find_technique("grid_thing");

  _freefly_camera.setPos(0, 0, -100);

  {
    TweakableParameterBlock block("spring");
    block._params.emplace_back(TweakableParameter("stiffness", _stiffness, 0.1f, 25.0f));
    block._params.emplace_back(TweakableParameter("dampening", _dampening, 0.001f, 1.0f));
    block._params.emplace_back(TweakableParameter("force", _force, 0.25f, 10.0f));

    APP.add_parameter_block(block, true, [=](const JsonValue::JsonValuePtr &msg) {
      _stiffness = (float)msg->get("stiffness")->get("value")->to_number();
      _dampening = (float)msg->get("dampening")->get("value")->to_number();
      _force = (float)msg->get("force")->get("value")->to_number();
    });
  }

  return true;
}

void GridThing::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  *proj = _freefly_camera.projectionMatrix();

  if (_useFreeflyCamera) {
    _cameraPos = _freefly_camera.pos();
    *view = _freefly_camera.viewMatrix();
  } else {
  }
}

bool GridThing::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time / 1000.0;
  _curTime = local_time / 1000.0;

  calc_camera_matrices(time, delta_ns / 1e6, &_view, &_proj);

  updateGrid((float)(delta_ns / 1e6));

  return true;
}

void GridThing::initGrid() {

  vector<int> indices(cHorizCells*cVertCells*6);

  int *p = indices.data();
  for (int i = 0; i < cVertCells; ++i) {
    for (int j = 0; j < cHorizCells; ++j) {
      // 2 3
      // 0 1
      int v0 = i*(cHorizCells+1) + j;
      int v1 = i*(cHorizCells+1) + j+1;
      int v2 = (i+1)*(cHorizCells+1) + j;
      int v3 = (i+1)*(cHorizCells+1) + j+1;

      *p++ = v0;
      *p++ = v2;
      *p++ = v3;

      *p++ = v0;
      *p++ = v3;
      *p++ = v1;
    }
  }
  _ib = GFX_create_buffer(D3D11_BIND_INDEX_BUFFER, 4*cHorizCells*cVertCells*6, false, indices.data(), DXGI_FORMAT_R32_UINT);

  const float xStep = 2.0f / cHorizCells;
  const float yStep = -2.0f / cVertCells;
  const float uStep = 1.0f / cHorizCells;
  const float vStep = 1.0f / cVertCells;

  float y = 1;
  float v = 0;
  for (int i = 0; i < cVertCells+1; ++i) {
    float x = -1;
    float u = 0;
    for (int j = 0; j < cHorizCells+1; ++j) {
      _grid(i,j) = Pos4Tex(x, y, 1, 1, u, v);
      _particles(i,j) = Particle(XMFLOAT3(x, y, 0), XMFLOAT3(0,0,0), XMFLOAT3(0,0,0));
      x += xStep;
      u += uStep;
    }
    y += yStep;
    v += vStep;
  }
}

void GridThing::updateGrid(float delta) {

  float k = _stiffness;
  float dampening = _dampening;

  float m = 1;

  for (int i = 0; i < cVertCells+1; ++i) {
    for (int j = 0; j < cHorizCells; ++j) {
      auto &cur = _particles(i,j);
      XMFLOAT3 F = k * (cur.orgPos - cur.pos) - dampening * cur.vel + _forces(i,j);
      XMFLOAT3 a = 1.0f / m * F;
      cur.vel += delta * a;
      cur.pos += delta * cur.vel + 1.0f/2 * delta*delta* a;

      _grid(i,j).pos = expand(cur.pos,1);
    }
  }

  ZeroMemory(_forces.data(), _forces.dataSize());

  ScopedMap<Pos4Tex> mappedVb(_ctx, D3D11_MAP_WRITE_DISCARD, _vb);
  memcpy(mappedVb.data(), _grid.data(), _grid.dataSize());
}

bool GridThing::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx->set_default_render_target(true);

  Technique *technique;
  Shader *vs, *ps;
  setupTechnique(_ctx, _technique, false, &technique, &vs, nullptr, &ps);

  _ctx->set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _ctx->set_vb(_vb);
  _ctx->set_ib(_ib);
  _ctx->set_shader_resource(_texture, ShaderType::kPixelShader);
  _ctx->set_samplers(ps->samplers());
  _ctx->draw_indexed(cHorizCells*cVertCells*6, 0, 0);

  _ctx->end_frame();

  return true;
}

bool GridThing::close() {
  return true;
}

void GridThing::addForce(int x, int y) {
  float xx = (float)x / GRAPHICS.width() * cHorizCells;
  float yy = (float)y / GRAPHICS.height() * cVertCells;

  int x0 = (int)xx;
  int x1 = min(cHorizCells, x0 + 1);
  int y0 = (int)yy;
  int y1 = min(cVertCells, y0 + 1);

  float fx = xx - x0;
  float nx = 1 - fx;
  float fy = yy - y0;
  float ny = 1 - fy;

  float s = _force;

  XMFLOAT3 p(xx, yy, 1);

  // 0 1
  // 2 3
  // pull 4 closest cells towards the force
  float fx0 = (float)x0;
  float fx1 = (float)x1;
  float fy0 = (float)y0;
  float fy1 = (float)y1;
  _forces(y0, x0) += s * normalize(p - XMFLOAT3(fx0, fy0, 1)); // s * (fx * XMFLOAT3(+1,0,0) + fy * XMFLOAT3(0,-1,0));
  _forces(y0, x1) += s * normalize(p - XMFLOAT3(fx1, fy0, 1)); // s * (nx * XMFLOAT3(-1,0,0) + fy * XMFLOAT3(0,-1,0));
  _forces(y1, x0) += s * normalize(p - XMFLOAT3(fx0, fy1, 1)); // s * (fx * XMFLOAT3(+1,0,0) + ny * XMFLOAT3(0,+1,0));
  _forces(y1, x1) += s * normalize(p - XMFLOAT3(fx1, fy1, 1)); // s * (nx * XMFLOAT3(-1,0,0) + ny * XMFLOAT3(0,+1,0));
}

void GridThing::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
    case WM_LBUTTONDOWN:
      if (_mouse_pos_prev != ~0)
        addForce(LOWORD(_mouse_pos_prev), HIWORD(_mouse_pos_prev));
      break;
    case WM_MOUSEMOVE:
      if (_mouse_lbutton && _mouse_pos_prev != ~0)
        addForce(LOWORD(_mouse_pos_prev), HIWORD(_mouse_pos_prev));
      break;
  }
}
