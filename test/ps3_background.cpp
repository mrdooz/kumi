#include "stdafx.h"
#include "ps3_background.hpp"
#include "../logger.hpp"
#include "../graphics.hpp"

bool Ps3BackgroundEffect::init() {
  LOG_VERBOSE_LN(__FUNCTION__);

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/ps3background.tec", true));
  //B_ERR_BOOL(GRAPHICS.load_techniques("effects/bloom.tec", true));

  _background_technique = GRAPHICS.find_technique("ps3background");
  //_bloom_technique = GRAPHICS.find_technique("bloom");
  //_bloom_rt = GRAPHICS.create_render_target(FROM_HERE, GRAPHICS.width(), GRAPHICS.height(), true, "bloom_rt");
  return true;
}

bool Ps3BackgroundEffect::update(int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  return true;
}

bool Ps3BackgroundEffect::render() {
/*
  RenderKey key;
  key.cmd = RenderKey::kSetRenderTarget;
  key.handle = _bloom_rt;
  RENDERER.submit_command(FROM_HERE, key, nullptr);
*/
  GRAPHICS.submit_technique(_background_technique);
/*
  key.handle = GRAPHICS.default_render_target();
  RENDERER.submit_command(FROM_HERE, key, nullptr);
  {
    RenderKey key;
    key.cmd = RenderKey::kRenderTechnique;
    TechniqueRenderData *data = RENDERER.alloc_command_data<TechniqueRenderData>();
    data->technique = _bloom_technique;
    data->render_targets[0] = _bloom_rt;
    RENDERER.submit_command(FROM_HERE, key, data);
  }
*/
  return true;
}

bool Ps3BackgroundEffect::close() {
  return true;
}
