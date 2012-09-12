#include "stdafx.h"
#include "scene_player.hpp"
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
#include "../dx_utils.hpp"

using namespace std;
using namespace std::tr1::placeholders;

static const int KERNEL_SIZE = 32;
static const int NOISE_SIZE = 16;

ScenePlayer::ScenePlayer(const std::string &name) 
  : Effect(name)
  , _scene(nullptr) 
  , _light_pos_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::LightPos"))
  , _light_color_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::LightColor"))
  , _light_att_start_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::AttenuationStart"))
  , _light_att_end_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::AttenuationEnd"))
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::proj"))
  , _ctx(nullptr)
  , _DofSettingsId(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::DOFDepths"))
  , _screenSizeId(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::screenSize"))
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

ScenePlayer::~ScenePlayer() {
  GRAPHICS.destroy_deferred_context(_ctx);
  delete exch_null(_scene);
}

bool ScenePlayer::file_changed(const char *filename, void *token) {
/*
  LOG_VERBOSE_LN(__FUNCTION__);
  KumiLoader loader;
  if (!loader.load(filename, &RESOURCE_MANAGER, &_scene))
    return false;
*/
  return true;
}

bool ScenePlayer::init() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  // from http://www.john-chapman.net/content.php?id=8
  XMFLOAT4 kernel[KERNEL_SIZE];
  for (int i = 0; i < KERNEL_SIZE; ++i) {
    // generate points on the unit hemisphere, then scale them so the majority are distributed closer to 0
    XMFLOAT3 tmp = normalize(XMFLOAT3(randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(0.05f, 1.0f)));
    float scale = (float)i / KERNEL_SIZE;
    scale = lerp(0.1f, 1.0f, scale * scale);
    kernel[i] = expand(scale * tmp, 0);
  }

  // generate random noise vectors rotated around the z-axis
  XMFLOAT4 noise[NOISE_SIZE];
  for (int i = 0; i < NOISE_SIZE; ++i) {
    noise[i] = expand(normalize(XMFLOAT3(randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), 0)), 0);
  }

  _kernel_id = PROPERTY_MANAGER.get_or_create_raw("System::kernel", sizeof(XMFLOAT4) * KERNEL_SIZE, kernel);
  _noise_id = PROPERTY_MANAGER.get_or_create_raw("System::noise", sizeof(XMFLOAT4) * NOISE_SIZE, noise);

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/default_shaders.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/ssao.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/luminance.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/blur.tec", true));

  B_ERR_BOOL(_blur.init());

  _ssao_fill = GRAPHICS.find_technique("ssao_fill");
  _ssao_ambient = GRAPHICS.find_technique("ssao_ambient");
  _ssao_light = GRAPHICS.find_technique("ssao_light");
  _ssao_compute = GRAPHICS.find_technique("ssao_compute");
  _ssao_blur = GRAPHICS.find_technique("ssao_blur");
  _default_shader = GRAPHICS.find_technique("default_shader");
  _gamma_correct = GRAPHICS.find_technique("gamma_correction");
  _luminance_map = GRAPHICS.find_technique("luminance_map");
  _scale = GRAPHICS.find_technique("scale");
  _scale_cutoff = GRAPHICS.find_technique("scale_cutoff");
  _blur_horiz = GRAPHICS.find_technique("blur_horiz");
  _blur_vert = GRAPHICS.find_technique("blur_vert");

  _rt_final = GRAPHICS.create_render_target(FROM_HERE, w, h, DXGI_FORMAT_R32G32B32A32_FLOAT, Graphics::kCreateSrv, "rt_final");

  //string material_connections = RESOURCE_MANAGER.resolve_filename("meshes/torus_materials.json", true);

  KumiLoader loader;
  if (!loader.load("meshes/greeble.kumi", nullptr /*material_connections.c_str()*/, &_scene))
    return false;

  _ambient_id = PROPERTY_MANAGER.get_or_create_raw("System::Ambient", sizeof(XMFLOAT4), nullptr);
  PROPERTY_MANAGER.set_property_raw(_ambient_id, &_scene->ambient.x, sizeof(XMFLOAT4));

  PROPERTY_MANAGER.set_property_raw(_ambient_id, &XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f), sizeof(XMFLOAT4));

  _ctx = GRAPHICS.create_deferred_context(true);

  // create properties from the materials
  for (auto it = begin(_scene->materials); it != end(_scene->materials); ++it) {
    Material *mat = *it;
    auto mp = unique_ptr<AnimatedTweakableParam>(new AnimatedTweakableParam(mat->name()));

    for (auto j = begin(mat->properties()); j != end(mat->properties()); ++j) {
      const Material::Property *prop = *j;
      switch (prop->type) {
        case PropertyType::kFloat: {
          AnimatedTweakableParam *p = new AnimatedTweakableParam(prop->name, AnimatedTweakableParam::kTypeFloat, prop->id);
          p->add_key(0, prop->_float4[0]);
          mp->add_child(p);
          break;
        }

        case PropertyType::kColor: {
          AnimatedTweakableParam *p = new AnimatedTweakableParam(prop->name, AnimatedTweakableParam::kTypeColor, prop->id);
          p->add_key(0, XMFLOAT4(prop->_float4));
          mp->add_child(p);
          break;
        }

        case PropertyType::kFloat4: {
          AnimatedTweakableParam *p = new AnimatedTweakableParam(prop->name, AnimatedTweakableParam::kTypeFloat4, prop->id);
          p->add_key(0, XMFLOAT4(prop->_float4));
          mp->add_child(p);
          break;
        }

      }
    }

    _params.push_back(move(mp));

    {
      TweakableParameterBlock block("blur");
      block._params.push_back(TweakableParameter("blurX", 10.0f, 1.0f, 250.0f));
      block._params.push_back(TweakableParameter("blurY", 10.0f, 1.0f, 250.0f));

      block._params.push_back(TweakableParameter("nearFocusStart", 10.0f, 1.0f, 2500.0f));
      block._params.push_back(TweakableParameter("nearFocusEnd", 50.0f, 1.0f, 2500.0f));
      block._params.push_back(TweakableParameter("farFocusStart", 100.0f, 1.0f, 2500.0f));
      block._params.push_back(TweakableParameter("farFocusEnd", 150.0f, 1.0f, 2500.0f));

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

  }

/*
  bool res;
  RESOURCE_MANAGER.add_file_watch(resolved_name.c_str(), NULL, bind(&ScenePlayer::file_changed, this, _1, _2), true, &res, 5000);
  return res;
*/
  return true;
}

template<typename T>
static T value_at_time(const vector<KeyFrame<T>> &frames, double time) {
  for (int i = 0; i < (int)frames.size() - 1; ++i) {
    if (time >= frames[i+0].time && time < frames[i+1].time) {
      return frames[i].value;
    }
  }
  return frames.back().value;
}

void ScenePlayer::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  if (_useFreeflyCamera || _scene->cameras.empty()) {
    *proj = transpose(_freefly_camera.projectionMatrix());
    *view = transpose(_freefly_camera.viewMatrix());

  } else {
    Camera *camera = _scene->cameras[0];

    XMFLOAT3 pos, target;
    if (!camera->is_static) {
      pos = ANIMATION_MANAGER.get_pos(camera->pos_id);
      target = ANIMATION_MANAGER.get_pos(camera->target_pos_id);
    } else {
      pos = camera->pos;
      target = camera->target;
    }
    // value_at_time(_scene->animation_vec3[camera->name + ".Target"], time);

    XMMATRIX xlookat = XMMatrixTranspose(XMMatrixLookAtLH(
      XMVectorSet(pos.x, pos.y, pos.z,0),
      XMVectorSet(target.x, target.y, target.z,0),
      XMVectorSet(0, 1, 0, 0)
      ));

    XMMATRIX xproj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(
      camera->fov_y,
      camera->aspect_ratio,
      camera->near_plane, camera->far_plane));

    XMStoreFloat4x4(view, xlookat);
    XMStoreFloat4x4(proj, xproj);

    *proj = transpose(perspective_foh(camera->fov_y, camera->aspect_ratio, camera->near_plane, camera->far_plane));
  }
}

bool ScenePlayer::update(int64 global_time, int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  if (!_scene)
    return true;

  double time = local_time  / 1000.0;

  ANIMATION_MANAGER.update(time);

  _scene->update();

  calc_camera_matrices(time, (double)delta / 1000000.0, &_view, &_proj);

  PROPERTY_MANAGER.set_property(_view_mtx_id, _view);
  PROPERTY_MANAGER.set_property(_proj_mtx_id, _proj);

  return true;
}

#pragma pack(push, 1)
struct LightBase {
  uint32 id;
  int len;
};

struct LightPos : public LightBase {
#pragma warning(suppress: 4200)
  XMFLOAT4 pos[];
};

struct LightColor : public LightBase {
#pragma warning(suppress: 4200)
  XMFLOAT4 color[];
};

struct LightAttenuationStart : public LightBase {
#pragma warning(suppress: 4200)
  float start[];
};

struct LightAttenuationEnd : public LightBase {
#pragma warning(suppress: 4200)
  float end[];
};
#pragma pack(pop)

bool ScenePlayer::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();
  if (_scene) {

    // allocate the required render targets

    int w = GRAPHICS.width();
    int h = GRAPHICS.height();

    ScopedRt rt_pos(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, Graphics::kCreateSrv | Graphics::kCreateDepthBuffer, "System::rt_pos");
    ScopedRt rt_normal(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, Graphics::kCreateSrv, "System::rt_normal");
    ScopedRt rt_diffuse(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Graphics::kCreateSrv, "System::rt_diffuse");
    ScopedRt rt_specular(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, Graphics::kCreateSrv, "System::rt_specular");

    ScopedRt rt_composite(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, Graphics::kCreateSrv, "System::rt_composite");
    ScopedRt rt_blur(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, Graphics::kCreateSrv, "System::rt_blur");

    ScopedRt rt_occlusion(w, h, DXGI_FORMAT_R16_FLOAT, Graphics::kCreateSrv, "System::rt_occlusion");
    ScopedRt rt_occlusion_tmp(w/2, h/2, DXGI_FORMAT_R16_FLOAT, Graphics::kCreateSrv, "System::rt_occlusion_tmp");

    ScopedRt rt_luminance(1024, 1024, DXGI_FORMAT_R16_FLOAT, Graphics::kCreateMipMaps | Graphics::kCreateSrv, "System::rt_luminance");

    {
      ADD_NAMED_PROFILE_SCOPE("render_meshes");
      GraphicsObjectHandle render_targets[] = { rt_pos, rt_normal, rt_diffuse, rt_specular };
      bool clear[] = { true, true, true, true };
      _ctx->set_render_targets(render_targets, clear, 4);
      _scene->render(_ctx, this, _ssao_fill);
    }

    {
      // Calc the occlusion
      _ctx->set_render_target(rt_occlusion_tmp, true);
      TextureArray arr = { rt_pos, rt_normal };
      _ctx->render_technique(_ssao_compute, bind(&ScenePlayer::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());
      _ctx->unset_render_targets(0, 1);
    }


    // Downscale and blur the occlusion
    //ScopedRt rt_occlusion_downscale(w/4, h/4, DXGI_FORMAT_R16_FLOAT, Graphics::kCreateSrv, "rt_occlusion_downscale");
    post_process(rt_occlusion_tmp, rt_occlusion, _ssao_blur);

    // Render Ambient * occlusion
    post_process(rt_occlusion, rt_composite, _ssao_ambient);

    {
      _ctx->set_render_target(rt_composite, false);

      DeferredContext::InstanceData data;
      int num_lights = (int)_scene->lights.size();
      data.add_variable(_light_pos_id, sizeof(XMFLOAT4));
      data.add_variable(_light_color_id, sizeof(XMFLOAT4));
      data.add_variable(_light_att_start_id, sizeof(float));
      data.add_variable(_light_att_end_id, sizeof(float));
      data.alloc(num_lights);

      for (size_t i = 0; i < _scene->lights.size(); ++i) {

        XMFLOAT4 *lightpos = (XMFLOAT4 *)data.data(0, i);
        XMFLOAT4 *lightcolor = (XMFLOAT4 *)data.data(1, i);
        float *lightattstart = (float *)data.data(2, i);
        float *lightattend = (float *)data.data(3, i);

        // transform pos to camera space
        auto light = _scene->lights[i];
        XMVECTOR v = XMLoadFloat4(&light->pos);
        XMFLOAT4X4 tmp = transpose(_view);
        XMMATRIX m = XMLoadFloat4x4(&tmp);
        XMVECTOR v2 = XMVector3Transform(v, m);
        XMStoreFloat4(lightpos, v2);
        *lightcolor = light->color;
        *lightattstart = light->far_attenuation_start;
        *lightattend = light->far_attenuation_end;
      }

      TextureArray arr = { rt_pos, rt_normal, rt_diffuse, rt_specular, rt_occlusion };
      _ctx->render_technique(_ssao_light, bind(&ScenePlayer::fill_cbuffer, this, _1), arr, data);
      _ctx->unset_render_targets(0, 1);
    }

    auto fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;

/*
    // calc luminance
    post_process(rt_composite, rt_luminance, _luminance_map);
    _ctx->generate_mips(rt_luminance);

    ScopedRt downscale1(w/2, h/2, fmt, Graphics::kCreateSrv, "downscale1");
    ScopedRt downscale2(w/4, h/4, fmt, Graphics::kCreateSrv, "downscale2");
    ScopedRt downscale3(w/8, h/8, fmt, Graphics::kCreateSrv, "downscale3");
    ScopedRt blur_tmp(w/8, h/8, fmt, Graphics::kCreateSrv, "blur_tmp");

    // scale down
    {
      _ctx->set_render_target(rt_composite, true);
      TextureArray arr = { downscale1, rt_luminance };
      _ctx->render_technique(_scale_cutoff, bind(&ScenePlayer::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());
      //post_process(rt_composite, downscale1, _scale_cutoff);
      _ctx->unset_render_targets(0, 1);
    }
    post_process(downscale1, downscale2, _scale);
    post_process(downscale2, downscale3, _scale);

    // apply blur
    for (int i = 0; i < 4; ++i) {
      post_process(downscale3, blur_tmp, _blur_horiz);
      post_process(blur_tmp, downscale3, _blur_vert);
    }

    // scale up
    post_process(downscale3, downscale2, _scale);
    post_process(downscale2, downscale1, _scale);

    int w4 = w/4;
    int h4 = h/4;
    ScopedRt rt_tmp(w4, h4, fmt, Graphics::kCreateSrv, "rt_tmp");
    post_process(rt_composite, rt_tmp, _scale);

    ScopedRt rt_tmpa(w4, h4, fmt, Graphics::kCreateUav | Graphics::kCreateSrv, "rt_tmpa");
    ScopedRt rt_tmpb(w4, h4, fmt, Graphics::kCreateUav | Graphics::kCreateSrv, "rt_tmpb");

    _ctx->set_vs(GraphicsObjectHandle());
    _ctx->set_ps(GraphicsObjectHandle());
    _ctx->set_gs(GraphicsObjectHandle());
    _blur.do_blur(0, rt_tmp, rt_tmpa, rt_tmpb, w4, h4, _ctx);

    _ctx->set_cs(GraphicsObjectHandle());
*/
    post_process(rt_composite, GraphicsObjectHandle(), _scale);
/*
    {
      // gamma correction
      _ctx->set_default_render_target(false);
      TextureArray arr = { rt_composite, _rt_final, rt_tmpb, rt_pos };
      _ctx->render_technique(_gamma_correct, 
        bind(&ScenePlayer::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());
    }
*/
  }

  _ctx->end_frame();

  return true;
}

void ScenePlayer::post_process(GraphicsObjectHandle input, GraphicsObjectHandle output, GraphicsObjectHandle technique) {
  if (output.is_valid())
    _ctx->set_render_target(output, true);
  else
    _ctx->set_default_render_target(false);

  TextureArray arr = { input };
  _ctx->render_technique(technique, bind(&ScenePlayer::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());

  if (output.is_valid())
    _ctx->unset_render_targets(0, 1);
}

bool ScenePlayer::close() {
  return true;
}

void ScenePlayer::update_from_json(const JsonValue::JsonValuePtr &state) {
  Effect::update_from_json(state);

  deque<pair<JsonValue::JsonValuePtr, JsonValue::JsonValuePtr>> param_stack;

  if (state->has_key("params")) {
    auto params = state->get("params");
    for (int i = 0; i < params->count(); ++i)
      param_stack.push_back(make_pair(JsonValue::emptyValue(), params->get(i)));

    while (!param_stack.empty()) {
      auto pp = param_stack.front();
      auto parent = pp.first;
      auto param = pp.second;

      if (param->has_key("keys")) {
        auto &param_name = param->get("name")->to_string();
        auto &first_key = param->get("keys")->get(0);
        auto &first_value = first_key->get("value");

        // this should be a material
        GraphicsObjectHandle mat_handle = MATERIAL_MANAGER.find_material(parent->get("name")->to_string());
        Material *cur_material = MATERIAL_MANAGER.get_material(mat_handle);
        KASSERT(cur_material);

        if (Material::Property *prop = cur_material->property_by_name(param_name)) {
          XMFLOAT4 value;
          auto type = param->get("type")->to_string();
          if (type == "float") {
            value.x = (float)first_value->get("x")->to_number();
            PROPERTY_MANAGER.set_property(prop->id, value.x);
          } else if (type == "color") {
            value.x = (float)first_value->get("r")->to_number();
            value.y = (float)first_value->get("g")->to_number();
            value.z = (float)first_value->get("b")->to_number();
            value.w = (float)first_value->get("a")->to_number();
            PROPERTY_MANAGER.set_property(prop->id, value);
          }
          memcpy(&prop->_float4, &value, sizeof(value));
        } else {
          LOG_WARNING_LN("Trying to set unknown property: %s (material: %s)", 
            param_name.c_str(), cur_material->name().c_str());
        }

      }

      if (param->has_key("children")) {
        auto children = param->get("children");
        for (int i = 0; i < children->count(); ++i)
          param_stack.push_back(make_pair(param, children->get(i)));
      }

      param_stack.pop_front();
    }
  }
}

void ScenePlayer::fill_cbuffer(CBuffer *cbuffer) const {
  for (size_t i = 0; i < cbuffer->vars.size(); ++i) {
    auto &cur = cbuffer->vars[i];
    if (cur.id == _DofSettingsId) {
      float *p = (float *)&cbuffer->staging[cur.ofs];
      p[0] = _nearFocusStart;
      p[1] = _nearFocusEnd;
      p[2] = _farFocusStart;
      p[3] = _farFocusEnd;
    } else if (cur.id == _screenSizeId) {
      float *p = (float *)&cbuffer->staging[cur.ofs];
      p[0] = (float)GRAPHICS.width();
      p[1] = (float)GRAPHICS.height();
      p[2] = (float)GRAPHICS.width() / 2;
      p[3] = (float)GRAPHICS.height() / 2;
    } else {
      PROPERTY_MANAGER.get_property_raw(cur.id, &cbuffer->staging[cur.ofs], cur.len);
    }
  }
}

