#pragma once

#include "graphics_submit.hpp"
#include "threading.hpp"

class Renderer {
public:
  static Renderer &instance();
  static bool create();
  static bool close();

  Renderer();
  template <typename T>
  T *alloc_command_data() {
    void *t = raw_alloc(sizeof(T));
    T *tt = new (t)T();
    return tt;
  }
  void *raw_alloc(size_t size);
  void *strdup(const char *str);
  void submit_command(const TrackedLocation &location, RenderKey key, void *data);
  void submit_technique(GraphicsObjectHandle technique);
  void render();

private:
  static Renderer *_instance;

  void validate_command(RenderKey key, const void *data);

  struct RenderCmd {
    RenderCmd(const TrackedLocation &location, RenderKey key, void *data) : location(location), key(key), data(data) {}
    TrackedLocation location;
    RenderKey key;
    void *data;
  };

  std::vector<RenderCmd > _render_commands;

  std::vector<uint8> _effect_data;
  int _effect_data_ofs;
};

#define RENDERER Renderer::instance()
