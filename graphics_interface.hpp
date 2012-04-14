#pragma once

struct TrackedLocation;

struct GraphicsInterface {

  virtual ~GraphicsInterface() {};
  virtual const char *vs_profile() const = 0;
  virtual const char *ps_profile() const = 0;

  virtual GraphicsObjectHandle create_constant_buffer(const TrackedLocation &loc, int size, bool dynamic) = 0;
  virtual GraphicsObjectHandle create_input_layout(const TrackedLocation &loc, const D3D11_INPUT_ELEMENT_DESC *desc, int elems, void *shader_bytecode, int len) = 0;
  virtual GraphicsObjectHandle create_static_vertex_buffer(const TrackedLocation &loc, uint32_t data_size, const void* data) = 0;
  virtual GraphicsObjectHandle create_static_index_buffer(const TrackedLocation &loc, uint32_t data_size, const void* data) = 0;

  virtual GraphicsObjectHandle create_dynamic_vertex_buffer(const TrackedLocation &loc, uint32_t size) = 0;

  virtual GraphicsObjectHandle create_vertex_shader(const TrackedLocation &loc, void *shader_bytecode, int len, const std::string &id) = 0;
  virtual GraphicsObjectHandle create_pixel_shader(const TrackedLocation &loc, void *shader_bytecode, int len, const std::string &id) = 0;

  virtual GraphicsObjectHandle create_rasterizer_state(const TrackedLocation &loc, const D3D11_RASTERIZER_DESC &desc) = 0;
  virtual GraphicsObjectHandle create_blend_state(const TrackedLocation &loc, const D3D11_BLEND_DESC &desc) = 0;
  virtual GraphicsObjectHandle create_depth_stencil_state(const TrackedLocation &loc, const D3D11_DEPTH_STENCIL_DESC &desc) = 0;
  virtual GraphicsObjectHandle create_sampler_state(const TrackedLocation &loc, const D3D11_SAMPLER_DESC &desc) = 0;

  virtual GraphicsObjectHandle get_or_create_font_family(const std::wstring &name) = 0;
};
