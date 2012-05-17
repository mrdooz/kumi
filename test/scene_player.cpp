#include "stdafx.h"
#include "scene_player.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_manager.hpp"
#include "../scene.hpp"
#include "../renderer.hpp"
#include "../threading.hpp"
#include "../mesh.hpp"

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
  LOG_VERBOSE_LN(__FUNCTION__);
  KumiLoader loader;
  if (!loader.load(filename, &RESOURCE_MANAGER, &_scene))
    return false;

  for (size_t i = 0; i < _scene->meshes.size(); ++i) {
    PROPERTY_MANAGER.set_property(_scene->meshes[i]->_world_mtx_id, transpose(_scene->meshes[i]->obj_to_world));
  }
  return true;
}

bool ScenePlayer::init() {

  auto p1 = new TweakableParam("param1", TweakableParam::kTypeBool, TweakableParam::kAnimStep);
  auto p2 = new TweakableParam("param2", TweakableParam::kTypeFloat, TweakableParam::kAnimSpline);

  p1->add_key(0, false);
  p1->add_key(1000, true);

  p2->add_key(0, 10.0f);
  p2->add_key(1000, 100.0f);
  p2->add_key(3000, -200.0f);
  p2->add_key(2000, 300.0f);

  _params.push_back(unique_ptr<TweakableParam>(p1));
  _params.push_back(unique_ptr<TweakableParam>(p2));

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/default_shaders.tec", true));

  string resolved_name = RESOURCE_MANAGER.resolve_filename("meshes\\torus.kumi");

  bool res;
  RESOURCE_MANAGER.add_file_watch(resolved_name.c_str(), NULL, bind(&ScenePlayer::file_changed, this, _1, _2), true, &res, 5000);
  return res;
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
