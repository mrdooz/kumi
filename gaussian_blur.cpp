#include "stdafx.h"
#include "gaussian_blur.hpp"
#include "technique.hpp"
#include "graphics.hpp"
#include "deferred_context.hpp"

bool GaussianBlur::init() {
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/blur.tec", true));
  _cs_blur_x = GRAPHICS.find_technique("hblur");
  _cs_blur_y = GRAPHICS.find_technique("vblur");

  _techniqueX = GRAPHICS.get_technique(_cs_blur_x);
  if (!_techniqueX)
    return false;

  _csX = _techniqueX->compute_shader(0);
  if (!_csX)
    return false;

  _cbufferX = _csX->find_cbuffer("blurSettings");

  _techniqueY = GRAPHICS.get_technique(_cs_blur_y);
  if (!_techniqueY)
    return false;

  _csY = _techniqueY->compute_shader(0);
  if (!_csY)
    return false;

  _cbufferY = _csY->find_cbuffer("blurSettings");

  return _cs_blur_x.is_valid() && _cs_blur_y.is_valid() && _cbufferX.is_valid() && _cbufferY.is_valid();
}

void GaussianBlur::do_blur(float amount, GraphicsObjectHandle inputTexture, GraphicsObjectHandle outputTexture, GraphicsObjectHandle outputTexture2, 
                           int width, int height, DeferredContext *ctx) {

  struct blurSettings {
    int dstWidth, dstHeight;
    float radius;
  } settings = { width, height, amount };

  int threadsPerGroup = 64;

  GraphicsObjectHandle src = inputTexture;
  GraphicsObjectHandle dst = outputTexture;

  auto swapBuffers = [&] {
    if (dst == outputTexture) {
      src = outputTexture;
      dst = outputTexture2;
    } else {
      src = outputTexture2;
      dst = outputTexture;
    }
  };

  for (int i = 0; i < 3; ++i) {
    {
      // blur x
      ctx->set_cs(_csX->handle());
      TextureArray arr = { src };
      ctx->set_shader_resources(arr, ShaderType::kComputeShader);

      TextureArray uav = { dst };
      ctx->set_uavs(uav);

      ctx->set_cbuffer(_cbufferX, 0, ShaderType::kComputeShader, &settings, sizeof(settings));
      ctx->dispatch((settings.dstHeight + threadsPerGroup-1) / threadsPerGroup, 1, 1);

      ctx->unset_shader_resource(0, 1, ShaderType::kComputeShader);
      ctx->unset_uavs(0, 1);
    }

    swapBuffers();

    {
      // blur y
      ctx->set_cs(_csY->handle());
      TextureArray arr = { src };
      ctx->set_shader_resources(arr, ShaderType::kComputeShader);

      TextureArray uav = { dst };
      ctx->set_uavs(uav);
      ctx->set_cbuffer(_cbufferY, 0, ShaderType::kComputeShader, &settings, sizeof(settings));
      ctx->dispatch((settings.dstWidth + threadsPerGroup-1) / threadsPerGroup, 1, 1);

      ctx->unset_shader_resource(0, 1, ShaderType::kComputeShader);
      ctx->unset_uavs(0, 1);
    }

    swapBuffers();
  }
}
