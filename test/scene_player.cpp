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

using namespace std;
using namespace std::tr1::placeholders;

XMFLOAT4X4 transpose(const XMFLOAT4X4 &mtx) {
  return XMFLOAT4X4(
    mtx._11, mtx._21, mtx._31, mtx._41,
    mtx._12, mtx._22, mtx._32, mtx._42,
    mtx._13, mtx._23, mtx._33, mtx._43,
    mtx._14, mtx._24, mtx._34, mtx._44);
}

ScenePlayer::ScenePlayer(GraphicsObjectHandle context, const std::string &name) 
  : Effect(context, name)
  , _scene(nullptr) 
  , _light_pos_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("LightPos"))
  , _light_color_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4>("LightColor"))
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("proj"))
{
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

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/default_shaders.tec", true));

  string resolved_name = RESOURCE_MANAGER.resolve_filename("meshes\\torus.kumi");
  string material_connections = RESOURCE_MANAGER.resolve_filename("meshes\\torus_materials.json");

  KumiLoader loader;
  if (!loader.load(resolved_name.c_str(), material_connections.c_str(), &RESOURCE_MANAGER, &_scene))
    return false;

  for (size_t i = 0; i < _scene->meshes.size(); ++i) {
    PROPERTY_MANAGER.set_property(_scene->meshes[i]->_world_mtx_id, transpose(_scene->meshes[i]->obj_to_world));
  }

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


bool ScenePlayer::update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, 
                              float ticks_fraction) {

  if (!_scene)
    return true;

  double time = global_time / 1000.0;

  for (auto it = begin(_scene->animation_mtx); it != end(_scene->animation_mtx); ++it) {
    if (Mesh *mesh = _scene->find_mesh_by_name(it->first)) {
      XMFLOAT4X4 mtx = transpose(value_at_time(it->second, time));
      PROPERTY_MANAGER.set_property(mesh->_world_mtx_id, mtx);
    }
  }

  if (!_scene->cameras.empty()) {

    _scene->update();

    Camera *camera = _scene->cameras[0];

    XMFLOAT3 pos = value_at_time(_scene->animation_vec3[camera->name], time);
    XMFLOAT3 target = value_at_time(_scene->animation_vec3[camera->name + ".Target"], time);

    //XMFLOAT4X4 mtx = mtx_at_time(_scene->animation_mtx[camera->name], time);
    //XMMATRIX mm = XMMatrixTranspose(XMLoadFloat4x4(&mtx));

    XMMATRIX lookat = XMMatrixTranspose(XMMatrixLookAtLH(
      XMVectorSet(pos.x, pos.y, pos.z,0),
      XMVectorSet(target.x, target.y, target.z,0),
      XMVectorSet(0, 1, 0, 0)
      ));

    XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(
      XMConvertToRadians(camera->fov),
      camera->aspect_ratio,
      camera->near_plane, camera->far_plane));

    XMFLOAT4X4 lookat_tmp, proj_tmp;
    XMStoreFloat4x4(&lookat_tmp, lookat);
    XMStoreFloat4x4(&proj_tmp, proj);

    if (!_scene->lights.empty()) {
      PROPERTY_MANAGER.set_property(_light_pos_id, _scene->lights[0]->pos);
      PROPERTY_MANAGER.set_property(_light_color_id, _scene->lights[0]->color);
    }

    PROPERTY_MANAGER.set_property(_view_mtx_id, lookat_tmp);
    PROPERTY_MANAGER.set_property(_proj_mtx_id, proj_tmp);
  }

  return true;
}

bool ScenePlayer::render() {

  static XMFLOAT4 clear_white(1, 1, 1, 1);
  static XMFLOAT4 clear_black(0, 0, 0, 1);

  RenderKey key;

  if (_scene) {
    _scene->submit_meshes(FROM_HERE, RENDERER.next_seq_nr());
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
        auto &param_name = param->get("name")->get_string();
        auto &first_key = param->get("keys")->get(0);
        auto &first_value = first_key->get("value");

        // this should be a material
        GraphicsObjectHandle mat_handle = MATERIAL_MANAGER.find_material(parent->get("name")->get_string());
        Material *cur_material = MATERIAL_MANAGER.get_material(mat_handle);
        assert(cur_material);

        if (Material::Property *prop = cur_material->property_by_name(param_name)) {
          XMFLOAT4 value;
          auto type = param->get("type")->get_string();
          if (type == "float") {
            value.x = (float)first_value->get("x")->get_number();
            PROPERTY_MANAGER.set_property(prop->id, value.x);
          } else if (type == "color") {
            value.x = (float)first_value->get("r")->get_number();
            value.y = (float)first_value->get("g")->get_number();
            value.z = (float)first_value->get("b")->get_number();
            value.w = (float)first_value->get("a")->get_number();
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