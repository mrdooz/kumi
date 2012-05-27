#include "stdafx.h"
#include "scene_player.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_manager.hpp"
#include "../scene.hpp"
#include "../renderer.hpp"
#include "../threading.hpp"
#include "../mesh.hpp"
#include "../tweakable_param.hpp"
#include "../graphics.hpp"
#include "../material.hpp"
#include "../material_manager.hpp"
#include "../xmath.hpp"

using namespace std;
using namespace std::tr1::placeholders;

ScenePlayer::ScenePlayer(GraphicsObjectHandle context, const std::string &name) 
  : Effect(context, name)
  , _scene(nullptr) 
  , _light_pos_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("LightPos"))
  , _light_color_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("LightColor"))
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("proj"))
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

  _rt_pos = GRAPHICS.create_render_target(FROM_HERE, GRAPHICS.width(), GRAPHICS.height(), true, true, DXGI_FORMAT_R16G16B16A16_FLOAT, "rt_pos");
  _rt_normal = GRAPHICS.create_render_target(FROM_HERE, GRAPHICS.width(), GRAPHICS.height(), true, false, DXGI_FORMAT_R16G16B16A16_FLOAT, "rt_normal");

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/default_shaders.tec", true));

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/ssao.tec", true));
  _ssao_fill = GRAPHICS.find_technique("ssao_fill");
  _ssao_render = GRAPHICS.find_technique("ssao_render");
  string resolved_name = RESOURCE_MANAGER.resolve_filename("meshes/torus.kumi");
  string material_connections = RESOURCE_MANAGER.resolve_filename("meshes/torus_materials.json");

  KumiLoader loader;
  if (!loader.load(resolved_name.c_str(), nullptr/*material_connections.c_str()*/, &RESOURCE_MANAGER, &_scene))
    return false;
/*
  for (size_t i = 0; i < _scene->meshes.size(); ++i) {
    PROPERTY_MANAGER.set_property(_scene->meshes[i]->_world_mtx_id, transpose(_scene->meshes[i]->obj_to_world));
  }
*/
  // create properties from the materials
  for (auto it = begin(_scene->materials); it != end(_scene->materials); ++it) {
    Material *mat = *it;
    TweakableParam *mp = new TweakableParam(mat->name());

    for (auto j = begin(mat->properties()); j != end(mat->properties()); ++j) {
      const Material::Property *prop = *j;
      switch (prop->type) {
        case PropertyType::kFloat: {
          TweakableParam *p = new TweakableParam(prop->name, TweakableParam::kTypeFloat, prop->id);
          p->add_key(0, prop->value.x);
          mp->add_child(p);
          break;
        }

        case PropertyType::kColor: {
          TweakableParam *p = new TweakableParam(prop->name, TweakableParam::kTypeColor, prop->id);
          p->add_key(0, prop->value);
          mp->add_child(p);
          break;
        }

        case PropertyType::kFloat4: {
          TweakableParam *p = new TweakableParam(prop->name, TweakableParam::kTypeFloat4, prop->id);
          p->add_key(0, prop->value);
          mp->add_child(p);
          break;
        }

      }
    }

    _params.push_back(unique_ptr<TweakableParam>(mp));
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
      _freefly_camera.theta -= (float)delta;
    if (_keystate['E'])
      _freefly_camera.theta += (float)delta;

    if (_mouse_lbutton) {
      float dx = _mouse_horiz / 200.0f;
      float dy = _mouse_vert / 200.0f;
      _freefly_camera.rho -= dx;
      _freefly_camera.theta += dy;
    }

    _freefly_camera.dir = vec3_from_spherical(_freefly_camera.theta, _freefly_camera.rho);
    _freefly_camera.up = drop(XMFLOAT4(0,1,0,0) * mtx_from_axis_angle(_freefly_camera.dir, _freefly_camera.roll));
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

  if (!_scene)
    return true;

  double time = local_time  / 1000.0;

  _scene->update();

  for (auto it = begin(_scene->animation_mtx); it != end(_scene->animation_mtx); ++it) {
    if (Mesh *mesh = _scene->find_mesh_by_name(it->first)) {
      XMFLOAT4X4 mtx = transpose(value_at_time(it->second, time));
      PROPERTY_MANAGER.set_property(mesh->_world_mtx_id, mtx);
    }
  }

  XMFLOAT4X4 view, proj;
  calc_camera_matrices(time, (double)delta / 1000000.0, &view, &proj);

  if (!_scene->lights.empty()) {
    PROPERTY_MANAGER.set_property(_light_pos_id, _scene->lights[0]->pos);
    PROPERTY_MANAGER.set_property(_light_color_id, _scene->lights[0]->color);
  }

  PROPERTY_MANAGER.set_property(_view_mtx_id, view);
  PROPERTY_MANAGER.set_property(_proj_mtx_id, proj);

  return true;
}

bool ScenePlayer::render() {

  RenderKey key;

  if (_scene) {

    RenderTargetData *data = RENDERER.alloc_command_data<RenderTargetData>();
    data->render_targets[0] = _rt_pos;
    data->render_targets[1] = _rt_normal;
    data->clear_target[0] = true;
    data->clear_target[1] = true;

    RenderKey key;
    key.cmd = RenderKey::kSetRenderTarget;
    RENDERER.submit_command(FROM_HERE, key, data);

    _scene->submit_meshes(FROM_HERE, -1, _ssao_fill);

    // set default render target
    RENDERER.submit_command(FROM_HERE, key, nullptr);
    RENDERER.submit_technique(_ssao_render);

  }

  return true;
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
        assert(cur_material);

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
          prop->value = value;
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
