#include "stdafx.h"
#include "particle_test.hpp"
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

using namespace std;
using namespace std::tr1::placeholders;

float particleFade[512] = {0.0643f,0.0643f,0.0643f,0.0643f,0.0643f,0.0644f,0.0644f,0.0644f,0.0644f,0.0644f,0.0644f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0646f,0.0646f,0.0646f,0.0646f,0.0646f,0.0646f,0.0647f,0.0647f,0.0647f,0.0648f,0.0648f,0.0648f,0.0649f,0.0649f,0.0650f,0.0650f,0.0651f,0.0652f,0.0653f,0.0653f,0.0654f,0.0655f,0.0656f,0.0658f,0.0659f,0.0660f,0.0661f,0.0663f,0.0665f,0.0666f,0.0668f,0.0670f,0.0672f,0.0674f,0.0676f,0.0678f,0.0680f,0.0681f,0.0683f,0.0684f,0.0686f,0.0687f,0.0689f,0.0691f,0.0692f,0.0694f,0.0696f,0.0697f,0.0699f,0.0701f,0.0702f,0.0704f,0.0706f,0.0708f,0.0710f,0.0712f,0.0714f,0.0716f,0.0718f,0.0720f,0.0722f,0.0724f,0.0726f,0.0728f,0.0730f,0.0732f,0.0734f,0.0736f,0.0738f,0.0741f,0.0743f,0.0745f,0.0747f,0.0750f,0.0752f,0.0755f,0.0757f,0.0759f,0.0762f,0.0764f,0.0767f,0.0769f,0.0772f,0.0774f,0.0777f,0.0779f,0.0782f,0.0785f,0.0787f,0.0790f,0.0793f,0.0796f,0.0798f,0.0801f,0.0804f,0.0807f,0.0810f,0.0813f,0.0815f,0.0818f,0.0821f,0.0824f,0.0827f,0.0830f,0.0833f,0.0836f,0.0840f,0.0843f,0.0846f,0.0849f,0.0852f,0.0855f,0.0859f,0.0862f,0.0865f,0.0868f,0.0872f,0.0875f,0.0879f,0.0882f,0.0885f,0.0889f,0.0892f,0.0896f,0.0900f,0.0904f,0.0909f,0.0913f,0.0917f,0.0921f,0.0925f,0.0929f,0.0933f,0.0938f,0.0942f,0.0946f,0.0950f,0.0955f,0.0959f,0.0963f,0.0968f,0.0972f,0.0976f,0.0981f,0.0985f,0.0990f,0.0994f,0.0999f,0.1003f,0.1008f,0.1012f,0.1017f,0.1022f,0.1027f,0.1031f,0.1036f,0.1041f,0.1046f,0.1051f,0.1056f,0.1061f,0.1066f,0.1071f,0.1076f,0.1082f,0.1087f,0.1092f,0.1098f,0.1103f,0.1108f,0.1114f,0.1120f,0.1125f,0.1131f,0.1137f,0.1143f,0.1148f,0.1154f,0.1160f,0.1167f,0.1173f,0.1179f,0.1185f,0.1191f,0.1198f,0.1204f,0.1211f,0.1218f,0.1224f,0.1231f,0.1238f,0.1245f,0.1252f,0.1259f,0.1266f,0.1273f,0.1281f,0.1288f,0.1295f,0.1301f,0.1307f,0.1312f,0.1318f,0.1322f,0.1327f,0.1331f,0.1335f,0.1338f,0.1342f,0.1345f,0.1347f,0.1350f,0.1353f,0.1355f,0.1357f,0.1359f,0.1361f,0.1363f,0.1365f,0.1367f,0.1368f,0.1370f,0.1372f,0.1374f,0.1376f,0.1378f,0.1380f,0.1382f,0.1384f,0.1387f,0.1389f,0.1392f,0.1395f,0.1398f,0.1402f,0.1406f,0.1410f,0.1414f,0.1419f,0.1424f,0.1430f,0.1436f,0.1442f,0.1449f,0.1456f,0.1464f,0.1472f,0.1481f,0.1490f,0.1500f,0.1510f,0.1521f,0.1533f,0.1545f,0.1558f,0.1571f,0.1585f,0.1600f,0.1616f,0.1632f,0.1649f,0.1667f,0.1686f,0.1706f,0.1726f,0.1747f,0.1769f,0.1793f,0.1817f,0.1842f,0.1867f,0.1894f,0.1922f,0.1951f,0.1981f,0.2019f,0.2070f,0.2124f,0.2180f,0.2240f,0.2302f,0.2366f,0.2433f,0.2502f,0.2573f,0.2646f,0.2722f,0.2799f,0.2878f,0.2959f,0.3041f,0.3125f,0.3210f,0.3296f,0.3384f,0.3473f,0.3562f,0.3653f,0.3744f,0.3837f,0.3929f,0.4022f,0.4116f,0.4210f,0.4304f,0.4398f,0.4493f,0.4587f,0.4681f,0.4774f,0.4868f,0.4960f,0.5052f,0.5144f,0.5234f,0.5324f,0.5413f,0.5501f,0.5587f,0.5672f,0.5756f,0.5838f,0.5919f,0.5998f,0.6075f,0.6153f,0.6250f,0.6347f,0.6445f,0.6544f,0.6642f,0.6741f,0.6841f,0.6940f,0.7040f,0.7139f,0.7238f,0.7336f,0.7435f,0.7532f,0.7629f,0.7726f,0.7821f,0.7916f,0.8010f,0.8102f,0.8193f,0.8283f,0.8371f,0.8458f,0.8543f,0.8627f,0.8708f,0.8788f,0.8866f,0.8941f,0.9014f,0.9085f,0.9154f,0.9220f,0.9283f,0.9343f,0.9401f,0.9455f,0.9507f,0.9560f,0.9609f,0.9654f,0.9696f,0.9734f,0.9770f,0.9802f,0.9832f,0.9858f,0.9883f,0.9905f,0.9924f,0.9942f,0.9957f,0.9971f,0.9983f,0.9993f,1.0002f,1.0009f,1.0016f,1.0021f,1.0026f,1.0029f,1.0032f,1.0035f,1.0037f,1.0040f,1.0042f,1.0044f,1.0046f,1.0049f,1.0052f,1.0056f,1.0061f,1.0067f,1.0074f,1.0081f,1.0089f,1.0097f,1.0105f,1.0113f,1.0121f,1.0129f,1.0136f,1.0144f,1.0151f,1.0157f,1.0163f,1.0169f,1.0174f,1.0178f,1.0181f,1.0183f,1.0184f,1.0185f,1.0184f,1.0182f,1.0178f,1.0173f,1.0167f,1.0159f,1.0150f,1.0139f,1.0126f,1.0111f,1.0095f,1.0076f,1.0056f,1.0033f,1.0008f,0.9971f,0.9927f,0.9881f,0.9832f,0.9780f,0.9725f,0.9667f,0.9606f,0.9541f,0.9473f,0.9402f,0.9327f,0.9248f,0.9165f,0.9078f,0.8987f,0.8892f,0.8793f,0.8688f,0.8580f,0.8466f,0.8348f,0.8225f,0.8041f,0.7836f,0.7617f,0.7384f,0.7141f,0.6888f,0.6628f,0.6362f,0.6092f,0.5820f,0.5547f,0.5276f,0.5008f,0.4745f,0.4489f,0.4241f,0.3953f,0.3652f,0.3341f,0.3026f,0.2709f,0.2395f,0.2088f,0.1792f,0.1511f,0.1249f,0.1010f,0.0798f,0.0617f,0.0500f,0.0500f,0.0500f,0.0500f};
/*[{"x":0.5382888381545609,"y":0.06428571428571428},{"x":10.990988241659629,"y":0.06785714285714284},{"x":28.018015268686657,"y":0.0892857142857143},{"x":42.597373971786595,"y":0.12857142857142856},{"x":57.84952649331181,"y":0.19999999999999996},{"x":67.68962489429582,"y":0.6142857142857143},{"x":75.31570115505842,"y":0.95},{"x":82.32677126575953,"y":1.0071428571428571},{"x":89.09909634976773,"y":1},{"x":93.5585558092272,"y":0.8214285714285714},{"x":96.6666639173353,"y":0.42500000000000004},{"x":99.36936662003801,"y":0.050000000000000044}]*/

/*
[{"x":3.728985287899754,"y":0.8500000000000001},{"x":41.367361671663595,"y":0.8714285714285714},{"x":65.35260152406211,"y":0.8571428571428572},{"x":76.05370853513223,"y":0.6964285714285712},{"x":89.21484014644834,"y":-0.05357142857142838}]
*/

struct ParticleVtx {
  XMFLOAT3 pos;
  XMFLOAT2 scale;
};

static const int numParticles = 5000;

// a bit of a misnomer, as we don't call T's ctor
template<typename T>
T *aligned_new(int count, int alignment) {
  return (T *)_aligned_malloc(count * sizeof(T), alignment);
}

ParticleTest::ParticleData::ParticleData(int numParticles)
  : numParticles(numParticles)
  , pos(aligned_new<float>(3 * numParticles, 16))
  , posX(pos)
  , posY(pos + numParticles)
  , posZ(pos + 2 * numParticles)
  , vel(aligned_new<float>(3 * numParticles, 16))
  , velX(vel)
  , velY(vel + numParticles)
  , velZ(vel + 2 * numParticles)
  , radius(aligned_new<float>(numParticles, 16))
  , age(aligned_new<float>(numParticles, 16))
  , maxAge(aligned_new<float>(numParticles, 16))
  , factor(aligned_new<float>(numParticles, 16))
{
  for (int i = 0; i < numParticles; ++i) {
    initParticle(i);
  }
}

ParticleTest::ParticleData::~ParticleData() {
  _aligned_free(pos);
  _aligned_free(vel);
  _aligned_free(radius);
  _aligned_free(age);
  _aligned_free(maxAge);
  _aligned_free(factor);
}

inline void ParticleTest::ParticleData::initParticle(int i) {
  float v = 5;
  velX[i] = gaussianRand(0, v);
  velY[i] = gaussianRand(0, v);
  velZ[i] = gaussianRand(0, v);

  float s = 20;
  posX[i] = gaussianRand(0, s);
  posY[i] = gaussianRand(0, s);
  posZ[i] = gaussianRand(0, s);

  radius[i] = gaussianRand(2.0f, 1.0f);
  age[i] = 0;
  maxAge[i] = randf(15.0f, 45.0f);
}

void ParticleTest::ParticleData::update(float delta) {
  KASSERT((numParticles % 4) == 0);

  __m128 d = _mm_set_ps1(delta);

  // check if any particles have expired
  for (int i = 0; i < numParticles; i += 4) {
    __m128 newAge = _mm_add_ps(d, _mm_load_ps(&age[i]));
    _mm_store_ps(&age[i], newAge);

    for (int j = 0; j < 4; ++j) {
      if (age[i+j] >= maxAge[i+j]) {
        initParticle(i+j);
        age[i+j] = 0;
      }
    }
  }

  // update factors
  for (int i = 0; i < numParticles; ++i) {
    float f = ELEMS_IN_ARRAY(particleFade)*age[i]/maxAge[i];
    int m = (int)f;
    float a = particleFade[m];
    float b = particleFade[min(ELEMS_IN_ARRAY(particleFade)-1,m+1)];
    factor[i] = lerp(a, b, f-m);
  }

  // update position
  float *src = pos;
  float *v = vel;

  for (int i = 0; i < 3 * numParticles; i += 4) {
    _mm_store_ps(src, _mm_add_ps(_mm_load_ps(src),_mm_mul_ps(d, _mm_load_ps(v))));
    src += 4;
    v += 4;
  }

}

ParticleTest::ParticleTest(const std::string &name) 
  : Effect(name)
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::proj"))
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
  , _ctx(nullptr)
  , _particle_data(numParticles)
  , _useFreeFlyCamera(false)
  , _DofSettingsId(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::DOFDepths"))
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

ParticleTest::~ParticleTest() {
  GRAPHICS.destroy_deferred_context(_ctx);
}

bool ParticleTest::file_changed(const char *filename, void *token) {
  return true;
}


bool ParticleTest::init() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/particles.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/scale.tec", true));
  _particle_technique = GRAPHICS.find_technique("particles");
  _gradient_technique = GRAPHICS.find_technique("gradient");
  _compose_technique = GRAPHICS.find_technique("compose");
  _coalesce = GRAPHICS.find_technique("coalesce");

  _particle_texture = RESOURCE_MANAGER.load_texture("gfx/particle3.png", "", false, nullptr);

  _scale = GRAPHICS.find_technique("scale");

  _ctx = GRAPHICS.create_deferred_context(true);

  if (!_blur.init()) {
    return false;
  }

  _freefly_camera.pos.z = -50;

  _vb = GRAPHICS.create_dynamic_vertex_buffer(FROM_HERE, numParticles * sizeof(ParticleVtx));

  {
    TweakableParameterBlock block("blur");
    block._params.emplace_back(TweakableParameter("blurX", 10.0f, 1.0f, 250.0f));
    block._params.emplace_back(TweakableParameter("blurY", 10.0f, 1.0f, 250.0f));

    block._params.emplace_back(TweakableParameter("nearFocusStart", 10.0f, 1.0f, 2500.0f));
    block._params.emplace_back(TweakableParameter("nearFocusEnd", 50.0f, 1.0f, 2500.0f));
    block._params.emplace_back(TweakableParameter("farFocusStart", 100.0f, 1.0f, 2500.0f));
    block._params.emplace_back(TweakableParameter("farFocusEnd", 150.0f, 1.0f, 2500.0f));

    APP.add_parameter_block(block, true, [=](const JsonValue::JsonValuePtr &msg) {
      _blurX = (float)msg->get("blurX")->get("value")->to_number();
      _blurY = (float)msg->get("blurY")->get("value")->to_number();

      if (msg->has_key("nearFocusStart")) {
        _nearFocusStart = (float)msg->get("nearFocusStart")->get("value")->to_number();
        _nearFocusEnd = (float)msg->get("nearFocusEnd")->get("value")->to_number();
        _farFocusStart = (float)msg->get("farFocusStart")->get("value")->to_number();
        _farFocusEnd = (float)msg->get("farFocusEnd")->get("value")->to_number();
      }
    });
  }

  // run the simulation for 10 seconds to get an interesting starting position
  for (int i = 0; i < 1000; ++i) {
    _particle_data.update(0.01f);
  }

  return true;
}

void ParticleTest::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  float fov = 45 * 3.1415f / 180;
  float aspect = 1.6f;
  float zn = 1;
  float zf = 2500;
  *proj = transpose(perspective_foh(fov, aspect, zn, zf));

  if (_useFreeFlyCamera) {
    if (!is_bit_set(GetKeyState(VK_CONTROL), 15)) {
      if (_keystate['W'])
        _freefly_camera.pos += (float)(100 * delta) * _freefly_camera.dir;
      if (_keystate['S'])
        _freefly_camera.pos -= (float)(100 * delta) * _freefly_camera.dir;
      if (_keystate['A'])
        _freefly_camera.pos -= (float)(100 * delta) * _freefly_camera.right;
      if (_keystate['D'])
        _freefly_camera.pos += (float)(100 * delta) * _freefly_camera.right;
      if (_keystate['Q'])
        _freefly_camera.pos += (float)(100 * delta) * _freefly_camera.up;
      if (_keystate['E'])
        _freefly_camera.pos -= (float)(100 * delta) * _freefly_camera.up;
    }

    if (_mouse_lbutton) {
      float dx = (float)(100 * delta) * _mouse_horiz / 200.0f;
      float dy = (float)(100 * delta) * _mouse_vert / 200.0f;
      _freefly_camera.rho -= dx;
      _freefly_camera.theta += dy;
    }

    _freefly_camera.dir = vec3_from_spherical(_freefly_camera.theta, _freefly_camera.rho);
    _freefly_camera.right = cross(_freefly_camera.up, _freefly_camera.dir);
    _freefly_camera.up = cross(_freefly_camera.dir, _freefly_camera.right);

    // Camera mtx = inverse of camera -> world mtx
    // R^-1         0
    // -R^-1 * t    1

    XMFLOAT4X4 tmp(mtx_identity());
    set_col(_freefly_camera.right, 0, &tmp);
    set_col(_freefly_camera.up, 1, &tmp);
    set_col(_freefly_camera.dir, 2, &tmp);

    tmp._41 = -dot(_freefly_camera.right, _freefly_camera.pos);
    tmp._42 = -dot(_freefly_camera.up, _freefly_camera.pos);
    tmp._43 = -dot(_freefly_camera.dir, _freefly_camera.pos);

    *view = transpose(tmp);
  } else {

    XMVECTOR pos = XMLoadFloat4(&XMFLOAT4(0, 0, -50, 0));
    XMVECTOR at = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 0));
    float x = -cosf((float)time/20);
    float y = sinf((float)time/20);
    XMVECTOR up = XMLoadFloat4(&XMFLOAT4(x, y, 0, 0));
    XMMATRIX lookat = XMMatrixLookAtLH(pos, at, up);
    XMFLOAT4X4 tmp;
    XMStoreFloat4x4(&tmp, lookat);
    *view = transpose(tmp);
  }

}

bool ParticleTest::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time  / 1000.0;

  calc_camera_matrices(time, delta_ns / 1e6, &_view, &_proj);

  PROPERTY_MANAGER.set_property(_view_mtx_id, _view);
  PROPERTY_MANAGER.set_property(_proj_mtx_id, _proj);

  _particle_data.update(delta_ns / 1e6f);

  return true;
}

void ParticleTest::post_process(GraphicsObjectHandle input, GraphicsObjectHandle output, GraphicsObjectHandle technique) {
  if (output.is_valid())
    _ctx->set_render_target(output, true);
  else
    _ctx->set_default_render_target();

  TextureArray arr = { input };
  _ctx->render_technique(technique, bind(&ParticleTest::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());

  if (output.is_valid())
    _ctx->unset_render_targets(0, 1);
}

void ParticleTest::renderParticles() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  D3D11_MAPPED_SUBRESOURCE res;
  GRAPHICS.map(_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);

  ParticleVtx *verts = (ParticleVtx *)res.pData;
  float *px = _particle_data.posX;
  float *py = _particle_data.posY;
  float *pz = _particle_data.posZ;
  float *r = _particle_data.radius;
  float *f = _particle_data.factor;
  for (int i = 0; i < _particle_data.numParticles; ++i) {
    verts[i].pos = XMFLOAT3(*px++, *py++, *pz++);
    float s = _particle_data.age[i] / _particle_data.maxAge[i];
    verts[i].scale.x = s * *r++;
    verts[i].scale.y = *f++;
  }

  GRAPHICS.unmap(_vb, 0);

  //auto fmt = DXGI_FORMAT_R32G32B32A32_FLOAT;

  Technique *technique = GRAPHICS.get_technique(_particle_technique);
  Shader *vs = technique->vertex_shader(0);
  Shader *gs = technique->geometry_shader(0);
  Shader *ps = technique->pixel_shader(0);

  _ctx->set_rs(technique->rasterizer_state());
  _ctx->set_dss(technique->depth_stencil_state(), GRAPHICS.default_stencil_ref());
  _ctx->set_bs(technique->blend_state(), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

  _ctx->set_vs(vs->handle());
  _ctx->set_gs(gs->handle());
  _ctx->set_ps(ps->handle());

  _ctx->set_layout(vs->input_layout());
  _ctx->set_topology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

  TextureArray arr = { _particle_texture };
  _ctx->set_shader_resources(arr, ShaderType::kPixelShader);

  auto &samplers = ps->samplers();
  if (!samplers.empty()) {
    _ctx->set_samplers(samplers);
  }

  struct ParticleCBuffer {
    XMFLOAT4 cameraPos;
    XMFLOAT4X4 worldView;
    XMFLOAT4X4 proj;
    XMFLOAT4 dofSettings;
  } cbuffer;

  if (_useFreeFlyCamera) {
    cbuffer.cameraPos.x = _freefly_camera.pos.x;
    cbuffer.cameraPos.y = _freefly_camera.pos.y;
    cbuffer.cameraPos.z = _freefly_camera.pos.z;
  } else {
    cbuffer.cameraPos = XMFLOAT4(0,0,-50,0);
  }

  cbuffer.worldView = _view;
  cbuffer.proj = _proj;
  cbuffer.dofSettings = XMFLOAT4(_nearFocusStart, _nearFocusEnd, _farFocusStart, _farFocusEnd);

  _ctx->set_cbuffer(vs->find_cbuffer("ParticleBuffer"), 0, ShaderType::kVertexShader, &cbuffer, sizeof(cbuffer));
  _ctx->set_cbuffer(gs->find_cbuffer("ParticleBuffer"), 0, ShaderType::kGeometryShader, &cbuffer, sizeof(cbuffer));

  _ctx->set_vb(_vb, sizeof(ParticleVtx));
  _ctx->draw(numParticles, 0);

  _ctx->unset_render_targets(0, 1);

}

bool ParticleTest::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  ScopedRt rtBackground(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Graphics::kCreateSrv, "rtBackground");

  _ctx->set_render_target(rtBackground, true);
  _ctx->render_technique(_gradient_technique, 
    bind(&ParticleTest::fill_cbuffer, this, _1), TextureArray(), DeferredContext::InstanceData());
  _ctx->unset_render_targets(0, 1);

  auto fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
  ScopedRt rtTmp(w, h, fmt, Graphics::kCreateSrv, "rtTmp");
  ScopedRt rtDepth(w, h, DXGI_FORMAT_R16_FLOAT, Graphics::kCreateSrv, "rtDepth");

  GraphicsObjectHandle targets[] = { rtTmp, rtDepth };
  bool clears[] = { true, true };
  _ctx->set_render_targets(targets, clears, 2);

  renderParticles();

  // coalesce the diffuse, cut-off diffuse and depth into a single texture
  ScopedRt rtCoalesce(w, h, fmt, Graphics::kCreateSrv, "rtCoalesce");
  {
    _ctx->set_render_target(rtCoalesce, true);
    TextureArray arr = { rtTmp, rtDepth };
    _ctx->render_technique(_coalesce, bind(&ParticleTest::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());
    _ctx->unset_render_targets(0, 1);
  }


  ScopedRt rtDownscaled(w/4, h/4, fmt, Graphics::kCreateSrv, "rtDownscale");
  post_process(rtCoalesce, rtDownscaled, _scale);

  ScopedRt rtBlur(w/4, h/4, fmt, Graphics::kCreateUav | Graphics::kCreateSrv, "rtBlur");
  ScopedRt rtBlur2(w/4, h/4, fmt, Graphics::kCreateUav | Graphics::kCreateSrv, "rtBlur2");
  _blur.do_blur(_blurX, rtDownscaled, rtBlur, rtBlur2, w/4, h/4, _ctx);

  {
    TextureArray arr = { rtCoalesce, rtBlur2, rtBackground };
    _ctx->set_default_render_target();
    _ctx->render_technique(_compose_technique, bind(&ParticleTest::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());
  }

  _ctx->end_frame();

  return true;
}

bool ParticleTest::close() {
  return true;
}

void ParticleTest::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

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
      //_use_freefly_camera = !_use_freefly_camera;
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
