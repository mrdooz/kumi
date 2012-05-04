#include "stdafx.h"
#include "scene_player.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_manager.hpp"
#include "../scene.hpp"
#include "../renderer.hpp"
#include "../threading.hpp"

using namespace std;
using namespace std::tr1::placeholders;

ScenePlayer::ScenePlayer(GraphicsObjectHandle context, const std::string &name) 
  : Effect(context, name)
  , _scene(nullptr) {
}

bool ScenePlayer::file_changed(const char *filename, void *token) {
  LOG_VERBOSE_LN(__FUNCTION__);
  KumiLoader loader;
  if (!loader.load(filename, &RESOURCE_MANAGER, &_scene))
    return false;

  for (size_t i = 0; i < _scene->meshes.size(); ++i) {
    XMMATRIX mtx = XMMatrixTranspose(XMLoadFloat4x4(&_scene->meshes[i]->obj_to_world));
    XMFLOAT4X4 tmp;
    XMStoreFloat4x4(&tmp, mtx);
    PROPERTY_MANAGER.set_mesh_property((PropertyManager::Id)_scene->meshes[i], "world", tmp);
  }
  return true;
}

bool ScenePlayer::init() {

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/diffuse.tec", true));

  string resolved_name = RESOURCE_MANAGER.resolve_filename("meshes\\torus.kumi");
  bool res;
  RESOURCE_MANAGER.add_file_watch(resolved_name.c_str(), NULL, bind(&ScenePlayer::file_changed, this, _1, _2), true, &res);
  return res;

}
static XMFLOAT4X4 mtx_at_time(const vector<KeyFrame> &frames, double time) {
  for (int i = 0; i < (int)frames.size() - 1; ++i) {
    if (time >= frames[i+0].time && time < frames[i+1].time) {
      return frames[i].mtx;
    }
  }
  return frames.back().mtx;
}

bool ScenePlayer::update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, 
                              float ticks_fraction) {
  if (_scene && !_scene->cameras.empty()) {
    Camera *camera = _scene->cameras[0];

    XMFLOAT4X4 mtx = mtx_at_time(_scene->animation[camera->name], global_time / 1000.0);

    XMMATRIX lookat = XMMatrixTranspose(XMMatrixLookAtLH(
//      XMVectorSet(camera->pos.x,camera->pos.y,camera->pos.z,0),
      XMVectorSet(mtx._41,mtx._42,mtx._43,0),
      XMVectorSet(camera->target.x,camera->target.y,camera->target.z,0),
      XMVectorSet(camera->up.x,camera->up.y,camera->up.z,0)));

    XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(
      XMConvertToRadians(camera->fov),
      camera->aspect_ratio,
      camera->near_plane, camera->far_plane));

    XMFLOAT4X4 lookat_tmp, proj_tmp;
    XMStoreFloat4x4(&lookat_tmp, lookat);
    XMStoreFloat4x4(&proj_tmp, proj);

    if (!_scene->lights.empty()) {
      PROPERTY_MANAGER.set_system_property("LightPos", _scene->lights[0]->pos);
      PROPERTY_MANAGER.set_system_property("LightColor", _scene->lights[0]->color);
    }

    PROPERTY_MANAGER.set_system_property("view", lookat_tmp);
    PROPERTY_MANAGER.set_system_property("proj", proj_tmp);
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
