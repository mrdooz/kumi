#pragma once

#include "utils.hpp"
#include "property.hpp"
#include "graphics_object_handle.hpp"
#include "graphics_submit.hpp"
#include "property_manager.hpp"
#include "shader.hpp"

struct GraphicsInterface;
struct ResourceInterface;

using std::vector;
using std::string;

struct Material;
class Technique;

#pragma pack(push, 1)
struct RenderObjects {
  GraphicsObjectHandle vs, gs, ps;
  GraphicsObjectHandle layout;
  GraphicsObjectHandle rs, dss, bs;
  UINT stencil_ref;
  float blend_factors[4];
  UINT sample_mask;
  GraphicsObjectHandle samplers[8];
};
#pragma pack(pop)

class Technique {
  friend class TechniqueParser;
public:
  Technique();
  ~Technique();

  const string &name() const { return _name; }

  GraphicsObjectHandle input_layout() const { return _input_layout; }
  Shader *vertex_shader() { return _vertex_shader; }
  Shader *pixel_shader() { return _pixel_shader; }

  vector<CBuffer> &get_cbuffers() { return _constant_buffers; }

  void get_render_objects(RenderObjects *obj);

  GraphicsObjectHandle rasterizer_state() const { return _rasterizer_state; }
  GraphicsObjectHandle blend_state() const { return _blend_state; }
  GraphicsObjectHandle depth_stencil_state() const { return _depth_stencil_state; }
  GraphicsObjectHandle sampler_state(const char *name) const;

  GraphicsObjectHandle vb() const { return _vb; }
  GraphicsObjectHandle ib() const { return _ib; }
  int vertex_size() const { return _vertex_size; }
  DXGI_FORMAT index_format() const { return _index_format; }
  int index_count() const { return (int)_indices.size(); }

  bool init(GraphicsInterface *graphics, ResourceInterface *res);
  bool reload_shaders();
  bool init_shader(GraphicsInterface *graphics, ResourceInterface *res, Shader *shader);

  bool is_valid() const { return _valid; }
  const std::string &error_msg() const { return _error_msg; }

  const std::string &get_default_sampler_state() const { return _default_sampler_state; }
private:

  void add_error_msg(const char *fmt, ...);
  bool compile_shader(GraphicsInterface *graphics, Shader *shader);
  bool do_reflection(GraphicsInterface *graphics, Shader *shader, void *buf, size_t len, 
                     std::set<std::string> *used_params);

  string _name;
  Shader *_vertex_shader;
  Shader *_pixel_shader;

  GraphicsObjectHandle _input_layout;

  vector<CBuffer> _constant_buffers;

  int _vertex_size;
  vector<float> _vertices;
  DXGI_FORMAT _index_format;
  vector<int> _indices;
  GraphicsObjectHandle _vb;
  GraphicsObjectHandle _ib;

  string _default_sampler_state;
  GraphicsObjectHandle _rasterizer_state;
  std::vector<std::pair<std::string, GraphicsObjectHandle> > _sampler_states;
  GraphicsObjectHandle _blend_state;
  GraphicsObjectHandle _depth_stencil_state;

  CD3D11_RASTERIZER_DESC _rasterizer_desc;
  std::vector<std::pair<std::string, CD3D11_SAMPLER_DESC>> _sampler_descs;
  CD3D11_BLEND_DESC _blend_desc;
  CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;

  vector<Material *> _materials;

  std::string _error_msg;
  bool _valid;
};
