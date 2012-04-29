#pragma once

#include "../demo_engine.hpp"

struct Scene;

class ScenePlayer : public Effect {
public:

  ScenePlayer(GraphicsObjectHandle context, const std::string &name);
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 frequency, int32 num_ticks, 
                      float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
private:
  void file_changed(const char *filename, void *token);

  Scene *_scene;
};
