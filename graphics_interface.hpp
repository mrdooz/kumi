#pragma once

struct GraphicsInterface {

  virtual ~GraphicsInterface() {};
  virtual const char *vs_profile() const = 0;
  virtual const char *ps_profile() const = 0;

  virtual GraphicsObjectHandle create_constant_buffer(int size) = 0;
  virtual GraphicsObjectHandle create_input_layout(const D3D11_INPUT_ELEMENT_DESC *desc, int elems, void *shader_bytecode, int len) = 0;
  virtual GraphicsObjectHandle create_static_vertex_buffer(uint32_t data_size, const void* data) = 0;
  virtual GraphicsObjectHandle create_static_index_buffer(uint32_t data_size, const void* data) = 0;

  virtual GraphicsObjectHandle create_dynamic_vertex_buffer(uint32_t size) = 0;

  virtual GraphicsObjectHandle create_vertex_shader(void *shader_bytecode, int len, const std::string &id) = 0;
  virtual GraphicsObjectHandle create_pixel_shader(void *shader_bytecode, int len, const std::string &id) = 0;

  virtual GraphicsObjectHandle create_rasterizer_state(const D3D11_RASTERIZER_DESC &desc) = 0;
  virtual GraphicsObjectHandle create_blend_state(const D3D11_BLEND_DESC &desc) = 0;
  virtual GraphicsObjectHandle create_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC &desc) = 0;
  virtual GraphicsObjectHandle create_sampler_state(const D3D11_SAMPLER_DESC &desc) = 0;

  virtual GraphicsObjectHandle create_font_family(const char *name) = 0;
};
