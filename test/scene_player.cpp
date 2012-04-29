#include "stdafx.h"
#include "scene_player.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_manager.hpp"

using namespace std;
using namespace std::tr1::placeholders;

ScenePlayer::ScenePlayer(GraphicsObjectHandle context, const std::string &name) 
  : Effect(context, name)
  , _scene(nullptr) {
}

void ScenePlayer::file_changed(const char *filename, void *token) {

}

bool ScenePlayer::init() {

  LOG_VERBOSE_LN(__FUNCTION__);
  KumiLoader loader;
  string resolved_name = RESOURCE_MANAGER.resolve_filename("meshes\\torus.kumi");
  loader.load(resolved_name.c_str(), &RESOURCE_MANAGER, &_scene);

  RESOURCE_MANAGER.add_file_watch(resolved_name.c_str(), true, NULL, bind(&ScenePlayer::file_changed, this, _1, _2));

/*
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/volumetric.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/diffuse.tec", true));

  KumiLoader loader;
  loader.load("meshes\\torus.kumi", &RESOURCE_MANAGER, &_scene);

  for (size_t i = 0; i < _scene->meshes.size(); ++i) {
    XMMATRIX mtx = XMMatrixTranspose(XMLoadFloat4x4(&_scene->meshes[i]->obj_to_world));
    XMFLOAT4X4 tmp;
    XMStoreFloat4x4(&tmp, mtx);
    PROPERTY_MANAGER.set_mesh_property((PropertyManager::Id)_scene->meshes[i], "world", tmp);
  }

  MaterialManager &m = MATERIAL_MANAGER;
  Graphics &g = GRAPHICS;

  _technique_diffuse = g.find_technique("diffuse");

  _material_sky = m.find_material("sky::sky");
  _technique_sky = g.find_technique("sky");

  _material_occlude = m.find_material("volumetric_occluder::black");
  _technique_occlude = g.find_technique("volumetric_occluder");

  _material_shaft = m.add_material(new Material("volumetric_shaft::"), false);
  _technique_shaft = g.find_technique("volumetric_shaft");

  _technique_add = g.find_technique("volumetric_add");

  int w, h;
  g.size(&w, &h);
  _occluder_rt = g.create_render_target(FROM_HERE, w, h, true, "volumetric_occluder");
  _shaft_rt = g.create_render_target(FROM_HERE, w, h, true, "volumetric_shaft");
  B_ERR_BOOL(_occluder_rt.is_valid() && _shaft_rt.is_valid());
*/
  return true;
}

bool ScenePlayer::update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, 
                              float ticks_fraction) {
/*
  if (_scene && !_scene->cameras.empty()) {
    Camera *camera = _scene->cameras[0];
    XMMATRIX lookat = XMMatrixTranspose(XMMatrixLookAtLH(
      XMVectorSet(camera->pos.x,camera->pos.y,camera->pos.z,0),
      XMVectorSet(camera->target.x,camera->target.y,camera->target.z,0),
      XMVectorSet(camera->up.x,camera->up.y,camera->up.z,0)));

    XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(
      XMConvertToRadians(camera->fov),
      camera->aspect_ratio,
      camera->near_plane, camera->far_plane));

    XMFLOAT4X4 lookat_tmp, proj_tmp;
    XMStoreFloat4x4(&lookat_tmp, lookat);
    XMStoreFloat4x4(&proj_tmp, proj);

    PROPERTY_MANAGER.set_system_property("view", lookat_tmp);
    PROPERTY_MANAGER.set_system_property("proj", proj_tmp);
  }
*/
  return true;
}

bool ScenePlayer::render() {
#if 0
  static XMFLOAT4 clear_white(1, 1, 1, 1);
  static XMFLOAT4 clear_black(0, 0, 0, 1);

  RenderKey key;

  if (_scene) {
/*
    // set render target
    key.data = 0;
    key.cmd = RenderKey::kSetRenderTarget;
    key.handle = _occluder_rt;
    key.seq_nr = RENDERER.next_seq_nr();
    RENDERER.submit_command(FROM_HERE, key, (void *)&clear_white);

    // render skydome
    _scene->submit_mesh(FROM_HERE, "Sphere001", RENDERER.next_seq_nr(), _material_sky, _technique_sky);

    // Render the occluders

    // Use the same sequence number for all the meshes to sort by technique
    _scene->submit_meshes(FROM_HERE, RENDERER.next_seq_nr(), _material_occlude, _technique_occlude);

    // Render the light streaks
    key.handle = _shaft_rt;
    key.seq_nr = RENDERER.next_seq_nr();
    RENDERER.submit_command(FROM_HERE, key, (void *)&clear_black);

    RenderKey t_key;
    t_key.data = 0;
    t_key.cmd = RenderKey::kRenderTechnique;
    t_key.seq_nr = RENDERER.next_seq_nr();
    TechniqueRenderData *d = (TechniqueRenderData *)RENDERER.alloc_command_data(sizeof(TechniqueRenderData));
    d->technique = _technique_shaft;
    d->material_id = _material_shaft;
    RENDERER.submit_command(FROM_HERE, t_key, d);
*/
    // render scene as usual to default render target
    key.cmd = RenderKey::kSetRenderTarget;
    key.handle = GRAPHICS.default_rt_handle();
    key.seq_nr = RENDERER.next_seq_nr();
    RENDERER.submit_command(FROM_HERE, key, NULL);

    _scene->submit_meshes(FROM_HERE, RENDERER.next_seq_nr());

    // add shafts
  }
#endif
  return true;
}

bool ScenePlayer::close() {
  return true;
}
