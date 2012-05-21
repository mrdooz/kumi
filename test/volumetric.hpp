#pragma once

#include "../effect.hpp"

struct Scene;

class VolumetricEffect : public Effect {
public:

  VolumetricEffect(GraphicsObjectHandle context, const std::string &name);
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, 
                      float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
private:
  GraphicsObjectHandle _occluder_rt;
  GraphicsObjectHandle _shaft_rt;
  Scene *_scene;

  uint16 _material_occlude, _material_add, _material_shaft, _material_sky;
  GraphicsObjectHandle _technique_occlude, _technique_add, _technique_shaft, _technique_sky, _technique_diffuse;
};
