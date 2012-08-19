#pragma once
#include "graphics_object_handle.hpp"

class DeferredContext;
class Technique;
class Shader;

class GaussianBlur {
public:
  bool init();
  void do_blur(float amount, GraphicsObjectHandle inputTexture, GraphicsObjectHandle outputTexture, GraphicsObjectHandle outputTexture2, 
    int width, int height, DeferredContext *ctx);
private:
  GraphicsObjectHandle _cs_blur_x, _cs_blur_y;

  Technique *_techniqueX, *_techniqueY;
  Shader *_csX, *_csY;
  GraphicsObjectHandle _cbufferX, _cbufferY;
};
