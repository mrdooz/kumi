#pragma once

#include "../effect.hpp"
#include "../property_manager.hpp"

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

  virtual void update_from_json(const JsonValue::JsonValuePtr &state) override;
  bool file_changed(const char *filename, void *token);

  PropertyManager::PropertyId _light_pos_id, _light_color_id;
  PropertyManager::PropertyId _view_mtx_id, _proj_mtx_id;

  Scene *_scene;
};
