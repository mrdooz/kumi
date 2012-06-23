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

class Material;
class Technique;

#pragma pack(push, 1)
struct RenderObjects {
  GraphicsObjectHandle vs, gs, ps;
  GraphicsObjectHandle layout;
  GraphicsObjectHandle rs, dss, bs;
  UINT stencil_ref;
  float blend_factors[4];
  UINT sample_mask;
  GraphicsObjectHandle samplers[MAX_SAMPLERS];
  int first_sampler;
  int num_valid_samplers;
};
#pragma pack(pop)

struct ShaderTemplate {
  friend TechniqueParser;
  enum Type {
    kVertexShader,
    kPixelShader,
    kGeometryShader,
  };

  ShaderTemplate(Type type) : _type(type) {}

  template<class T>
  T* find_by_name(const char *name, std::vector<T> &v) {
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i].name == name)
        return &v[i];
    }
    return NULL;
  }

private:
  Type _type;
  std::string _source_filename;
  std::string _obj_filename;
  std::string _entry_point;
  std::vector<CBufferParam> _cbuffer_params;
  std::vector<ResourceViewParam> _resource_view_params;
  std::vector<std::string> _flags;
};

class Technique {
  friend class TechniqueParser;
public:
  Technique();
  ~Technique();


  const string &name() const { return _name; }

  GraphicsObjectHandle input_layout() const { return _input_layout; }
  int vertex_shader_count() const { return (int)_vertex_shaders.size(); }
  int pixel_shader_count() const { return (int)_pixel_shaders.size(); }
  Shader *vertex_shader(int flags) { return _vertex_shaders[flags]; }
  Shader *pixel_shader(int flags) { return _pixel_shaders[flags]; }

  //vector<CBuffer> &get_cbuffers() { return _constant_buffers; }

  void get_render_objects(RenderObjects *obj, int vs_flags, int ps_flags);

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
  const TechniqueRenderData &render_data() const { return _render_data; }

  void fill_cbuffer(CBuffer *cbuffer);
  const std::vector<uint8> &cbuffer() const { return _cbuffer_staged; }
  GraphicsObjectHandle cbuffer_handle();

  std::vector<CBuffer> &get_cbuffer_vs() { return _vs_cbuffers; }
  std::vector<CBuffer> &get_cbuffer_ps() { return _ps_cbuffers; }

private:

  void prepare_cbuffers();

  CBufferParam *find_cbuffer_param(const std::string &name, Shader::Type type);

  std::vector<CBuffer> _vs_cbuffers;
  std::vector<CBuffer> _ps_cbuffers;

  std::vector<CBufferParam> _vs_cbuffer_params;
  std::vector<CBufferParam> _ps_cbuffer_params;

  void prepare_textures();

  void add_error_msg(const char *fmt, ...);
  bool compile_shader(GraphicsInterface *graphics, Shader *shader);
  bool do_reflection(GraphicsInterface *graphics, Shader *shader, void *buf, size_t len);
  bool do_text_reflection(const char *start, const char *end, GraphicsInterface *graphics, Shader *shader, void *buf, size_t len);

  bool parse_input_layout(GraphicsInterface *graphics, ID3D11ShaderReflection* reflector, const D3D11_SHADER_DESC &shader_desc, void *buf, size_t len);

  std::vector<CBufferVariable> _cbuffer_vars;
  std::vector<uint8> _cbuffer_staged;

  string _name;
  // we have multiple version of the shaders, one for each permutation of the compilation flags
  std::vector<Shader *> _vertex_shaders;
  std::vector<Shader *> _pixel_shaders;
  Shader *_vertex_shader_base;
  Shader *_pixel_shader_base;
  //Shader *_vertex_shader;
  //Shader *_pixel_shader;

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

  GraphicsObjectHandle _ordered_sampler_states[MAX_SAMPLERS];
  int _first_sampler;
  int _num_valid_samplers;


  CD3D11_RASTERIZER_DESC _rasterizer_desc;
  std::vector<std::pair<std::string, CD3D11_SAMPLER_DESC>> _sampler_descs;
  CD3D11_BLEND_DESC _blend_desc;
  CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;

  ShaderTemplate *_vs_shader_template;
  ShaderTemplate *_ps_shader_template;

  vector<Material *> _materials;

  std::string _error_msg;
  bool _valid;

  TechniqueRenderData _render_data;
};
