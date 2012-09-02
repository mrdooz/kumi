#pragma once

#include "../effect.hpp"
#include "../graphics_object_handle.hpp"

class Ps3BackgroundEffect : public Effect {
public:
  Ps3BackgroundEffect(const std::string &name) : Effect(name) {}
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 delta, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
private:
  GraphicsObjectHandle _background_technique;
  GraphicsObjectHandle _bloom_technique;

  GraphicsObjectHandle _bloom_rt;
};
