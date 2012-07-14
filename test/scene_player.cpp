#include "stdafx.h"
#include "scene_player.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_manager.hpp"
#include "../scene.hpp"
#include "../threading.hpp"
#include "../mesh.hpp"
#include "../tweakable_param.hpp"
#include "../graphics.hpp"
#include "../material.hpp"
#include "../material_manager.hpp"
#include "../xmath.hpp"
#include "../profiler.hpp"

using namespace std;
using namespace std::tr1::placeholders;

static const int KERNEL_SIZE = 32;
static const int NOISE_SIZE = 16;

ScenePlayer::ScenePlayer(GraphicsObjectHandle context, const std::string &name) 
  : Effect(context, name)
  , _scene(nullptr) 
  , _light_pos_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::LightPos"))
  , _light_color_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::LightColor"))
  , _light_att_start_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::AttenuationStart"))
  , _light_att_end_id(PROPERTY_MANAGER.get_or_create_placeholder("Instance::AttenuationEnd"))
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::proj"))
  , _use_freefly_camera(true)
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

ScenePlayer::~ScenePlayer() {
  delete exch_null(_scene);
}

bool ScenePlayer::file_changed(const char *filename, void *token) {
/*
  LOG_VERBOSE_LN(__FUNCTION__);
  KumiLoader loader;
  if (!loader.load(filename, &RESOURCE_MANAGER, &_scene))
    return false;

  for (size_t i = 0; i < _scene->meshes.size(); ++i) {
    PROPERTY_MANAGER.set_property(_scene->meshes[i]->_world_mtx_id, transpose(_scene->meshes[i]->obj_to_world));
  }
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

  string resolved_name = RESOURCE_MANAGER.resolve_filename("meshes/torus.kumi");
  string material_connections = RESOURCE_MANAGER.resolve_filename("meshes/torus_materials.json");

  KumiLoader loader;
  if (!loader.load(resolved_name.c_str(), material_connections.c_str(), &RESOURCE_MANAGER, &_scene))
    return false;

  _ambient_id = PROPERTY_MANAGER.get_or_create_raw("System::Ambient", sizeof(XMFLOAT4), nullptr);
  PROPERTY_MANAGER.set_property_raw(_ambient_id, &_scene->ambient.x, sizeof(XMFLOAT4));

  // create properties from the materials
  for (auto it = begin(_scene->materials); it != end(_scene->materials); ++it) {
    Material *mat = *it;
    auto mp = unique_ptr<TweakableParam>(new TweakableParam(mat->name()));

    for (auto j = begin(mat->properties()); j != end(mat->properties()); ++j) {
      const Material::Property *prop = *j;
      switch (prop->type) {
        case PropertyType::kFloat: {
          TweakableParam *p = new TweakableParam(prop->name, TweakableParam::kTypeFloat, prop->id);
          p->add_key(0, prop->_float4[0]);
          mp->add_child(p);
          break;
        }

        case PropertyType::kColor: {
          TweakableParam *p = new TweakableParam(prop->name, TweakableParam::kTypeColor, prop->id);
          p->add_key(0, XMFLOAT4(prop->_float4));
          mp->add_child(p);
          break;
        }

        case PropertyType::kFloat4: {
          TweakableParam *p = new TweakableParam(prop->name, TweakableParam::kTypeFloat4, prop->id);
          p->add_key(0, XMFLOAT4(prop->_float4));
          mp->add_child(p);
          break;
        }

      }
    }

    _params.push_back(move(mp));
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

  if (_use_freefly_camera || _scene->cameras.empty()) {
    float fov = 45 * 3.1415f / 180;
    float aspect = 1.6f;
    float zn = 1;
    float zf = 2500;
    *proj = transpose(perspective_foh(fov, aspect, zn, zf));

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

    if (_mouse_lbutton) {
      float dx = _mouse_horiz / 200.0f;
      float dy = _mouse_vert / 200.0f;
      _freefly_camera.rho -= dx;
      _freefly_camera.theta += dy;
    }

    _freefly_camera.dir = vec3_from_spherical(_freefly_camera.theta, _freefly_camera.rho);
    //_freefly_camera.up = drop(XMFLOAT4(0,1,0,0) * mtx_from_axis_angle(_freefly_camera.dir, _freefly_camera.roll));
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
    Camera *camera = _scene->cameras[0];

    XMFLOAT3 pos = value_at_time(_scene->animation_vec3[camera->name], time);
    XMFLOAT3 target = value_at_time(_scene->animation_vec3[camera->name + ".Target"], time);

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

bool ScenePlayer::update(int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  if (!_scene)
    return true;

  double time = local_time  / 1000.0;

  _scene->update();

  for (auto it = begin(_scene->animation_mtx); it != end(_scene->animation_mtx); ++it) {
    if (Mesh *mesh = _scene->find_mesh_by_name(it->first)) {
      XMFLOAT4X4 mtx = value_at_time(it->second, time);
      PROPERTY_MANAGER.set_property(mesh->_world_mtx_id, transpose(mtx));
    }
  }

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
  if (_scene) {

    // allocate the required render targets

    int w = GRAPHICS.width();
    int h = GRAPHICS.height();

    auto tmp_rt = [&](bool depth, DXGI_FORMAT fmt, const string& name) {
      return GRAPHICS.get_temp_render_target(FROM_HERE, w, h, depth, fmt, false, name);
    };

    GraphicsObjectHandle rt_pos = tmp_rt(true, DXGI_FORMAT_R16G16B16A16_FLOAT, "System::rt_pos");
    GraphicsObjectHandle rt_normal = tmp_rt(false, DXGI_FORMAT_R16G16B16A16_FLOAT, "System::rt_normal");
    GraphicsObjectHandle rt_diffuse = tmp_rt(false, DXGI_FORMAT_R16G16B16A16_FLOAT, "System::rt_diffuse");
    GraphicsObjectHandle rt_specular = tmp_rt(false, DXGI_FORMAT_R16G16B16A16_FLOAT, "System::rt_specular");

    GraphicsObjectHandle rt_composite = tmp_rt(false, DXGI_FORMAT_R16G16B16A16_FLOAT, "System::rt_composite");
    GraphicsObjectHandle rt_blur = tmp_rt(false, DXGI_FORMAT_R16G16B16A16_FLOAT, "System::rt_blur");

    GraphicsObjectHandle rt_occlusion = tmp_rt(false, DXGI_FORMAT_R16_FLOAT, "System::rt_occlusion");
    GraphicsObjectHandle rt_occlusion_tmp = tmp_rt(false, DXGI_FORMAT_R16_FLOAT, "System::rt_occlusion_tmp");

    GraphicsObjectHandle rt_luminance = GRAPHICS.get_temp_render_target(FROM_HERE, 1024, 1024, false, DXGI_FORMAT_R16_FLOAT, true, "System::rt_luminance");

    {
      // Fill the g-buffer
      RenderTargetData *data = GRAPHICS.alloc_command_data<RenderTargetData>();
      data->render_targets[0] = rt_pos;
      data->render_targets[1] = rt_normal;
      data->render_targets[2] = rt_diffuse;
      data->render_targets[3] = rt_specular;
      data->clear_target[0] = true;
      data->clear_target[1] = true;
      data->clear_target[2] = true;
      data->clear_target[3] = true;
      GFX_SUBMIT_CMD(RenderKey(RenderKey::kSetRenderTarget), data);

      _scene->submit_meshes(FROM_HERE, -1, _ssao_fill);
    }

    // Calc the occlusion
    post_process(GraphicsObjectHandle(), rt_occlusion_tmp, _ssao_compute);

    // Blur the occlusion
    post_process(rt_occlusion_tmp, rt_occlusion, _ssao_blur);

    // Render Ambient * occlusion
    post_process(GraphicsObjectHandle(), rt_composite, _ssao_ambient);

    {
      // Add the lighting
      int num_lights = (int)_scene->lights.size();
      int pos_size = sizeof(LightBase) + num_lights * sizeof(XMFLOAT4);
      int color_size = sizeof(LightBase) + num_lights * sizeof(XMFLOAT4);
      int as_size = sizeof(LightBase) + num_lights * sizeof(float);
      int ae_size = sizeof(LightBase) + num_lights * sizeof(float);
      int data_size = pos_size + color_size + 2 * ae_size;
      TechniqueRenderData *rd = GRAPHICS.alloc_command_data<TechniqueRenderData>(data_size);
      rd->num_instances = num_lights;
      rd->num_instance_variables = 4;

      LightPos *lightpos = (LightPos *)&rd->payload[0];
      LightColor *lightcolor = (LightColor *)&rd->payload[pos_size];
      LightAttenuationStart *lightattstart = (LightAttenuationStart *)&rd->payload[pos_size + color_size];
      LightAttenuationEnd *lightattend = (LightAttenuationEnd *)&rd->payload[pos_size + color_size + as_size];

      lightpos->id = _light_pos_id;
      lightpos->len = sizeof(XMFLOAT4);

      lightcolor->id = _light_color_id;
      lightcolor->len = sizeof(XMFLOAT4);

      lightattstart->id = _light_att_start_id;
      lightattstart->len = sizeof(float);

      lightattend->id = _light_att_end_id;
      lightattend->len = sizeof(float);

      for (size_t i = 0; i < _scene->lights.size(); ++i) {
        // transform pos to camera space
        auto light = _scene->lights[i];
        XMVECTOR v = XMLoadFloat4(&light->pos);
        XMMATRIX m = XMLoadFloat4x4(&transpose(_view));
        XMVECTOR v2 = XMVector3Transform(v, m);
        XMStoreFloat4(&lightpos->pos[i], v2);
        lightcolor->color[i] = light->color;
        lightattstart->start[i] = light->far_attenuation_start;
        lightattend->end[i] = light->far_attenuation_end;
      }

      GFX_SUBMIT_TECH(_ssao_light, rd);
    }

    {
      // calc luminance
      post_process(rt_composite, rt_luminance, _luminance_map);

      // meh, this is getting stupid now.. the whole submit stuff is close to disappearing :P
      // or, actually, I should move to using DX deferred contexts, because that's what I'm
      // trying to emulate anyways..
      GFX_SUBMIT_CMD(RenderKey(RenderKey::kGenerateMips, rt_luminance), nullptr);
    }
    auto fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
    GraphicsObjectHandle downscale1 = GRAPHICS.get_temp_render_target(FROM_HERE, w/2, h/2, false, fmt, false, "downscale1");
    GraphicsObjectHandle downscale2 = GRAPHICS.get_temp_render_target(FROM_HERE, w/4, h/4, false, fmt, false, "downscale2");
    GraphicsObjectHandle downscale3 = GRAPHICS.get_temp_render_target(FROM_HERE, w/8, h/8, false, fmt, false, "downscale3");
    GraphicsObjectHandle blur_tmp = GRAPHICS.get_temp_render_target(FROM_HERE, w/8, h/8, false, fmt, false, "blur_tmp");

    {

      // scale down
      post_process(rt_composite, downscale1, _scale_cutoff);
      post_process(downscale1, downscale2, _scale);
      post_process(downscale2, downscale3, _scale);

      // apply blur
      for (int i = 0; i < 4; ++i) {
        post_process(downscale3, blur_tmp, _blur_horiz);
        post_process(blur_tmp, downscale3, _blur_horiz);
      }

      // scale up
      post_process(downscale3, downscale2, _scale);
      post_process(downscale2, downscale1, _scale);
    }

    {
      // gamma correction
      GFX_SUBMIT_CMD(RenderKey(RenderKey::kSetRenderTarget), nullptr);

      TechniqueRenderData *rd = GRAPHICS.alloc_command_data<TechniqueRenderData>();
      rd->user_textures = 1;
      rd->textures[1] = downscale1;
      GFX_SUBMIT_TECH(_gamma_correct, rd);
    }

    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, blur_tmp), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, downscale3), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, downscale2), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, downscale1), nullptr);

    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_pos), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_normal), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_diffuse), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_specular), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_composite), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_blur), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_occlusion), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_occlusion_tmp), nullptr);
    GFX_SUBMIT_CMD(RenderKey(RenderKey::kReleaseRenderTarget, rt_luminance), nullptr);
  }

  return true;
}

void ScenePlayer::post_process(GraphicsObjectHandle input, GraphicsObjectHandle output, GraphicsObjectHandle technique) {

  RenderTargetData *rt = GRAPHICS.alloc_command_data<RenderTargetData>();
  rt->render_targets[0] = output;
  rt->clear_target[0] = true;
  GFX_SUBMIT_CMD(RenderKey(RenderKey::kSetRenderTarget), rt);

  TechniqueRenderData *rd = nullptr;
  if (input.is_valid()) {
    rd = GRAPHICS.alloc_command_data<TechniqueRenderData>();
    rd->user_textures = 1;
    rd->textures[0] = input;
  }
  GFX_SUBMIT_TECH(technique, rd);
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

void ScenePlayer::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

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
      _use_freefly_camera = !_use_freefly_camera;
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
