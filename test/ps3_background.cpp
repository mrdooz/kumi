#include "stdafx.h"
#include "ps3_background.hpp"
#include "../logger.hpp"
#include "../renderer.hpp"

bool Ps3BackgroundEffect::init() {
  LOG_VERBOSE_LN(__FUNCTION__);

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/ps3background.tec", true));

  _technique = GRAPHICS.find_technique("ps3background");
	return true;
}

bool Ps3BackgroundEffect::update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, float ticks_fraction) {
	return true;
}

bool Ps3BackgroundEffect::render() {
  RENDERER.submit_technique(_technique);
	return true;
}

bool Ps3BackgroundEffect::close() {
	return true;
}
