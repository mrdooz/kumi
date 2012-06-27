#pragma once

#include "utils.hpp"
#include "property.hpp"
#include "graphics_object_handle.hpp"
#include "graphics_submit.hpp"
#include "property_manager.hpp"
#include "shader.hpp"

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

struct ParamBase {
  ParamBase(const std::string &name, PropertyType::Enum type, PropertySource::Enum source) 
    : name(name), type(type), source(source) {}
  std::string name;
  PropertyType::Enum type;
  PropertySource::Enum source;
};

struct CBufferParam : public ParamBase {
  CBufferParam(const std::string &name, PropertyType::Enum type, PropertySource::Enum source) : ParamBase(name, type, source){
    // system properties can be connected now, but for mesh and material we have to wait
    if (source == PropertySource::kSystem) {
      int len = HIWORD(type);
      if (!len) {
        switch (type) {
        case PropertyType::kFloat: id = PROPERTY_MANAGER.get_or_create<float>(name.c_str()); break;
        case PropertyType::kFloat4: id = PROPERTY_MANAGER.get_or_create<XMFLOAT4>(name.c_str()); break;
        case PropertyType::kFloat4x4: id = PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>(name.c_str()); break;
        default: assert(!"Unknown type!"); break;
        }
      } else {
        switch (LOWORD(type)) {
        case PropertyType::kFloat: id = PROPERTY_MANAGER.get_or_create_raw(name.c_str(), sizeof(float) * len, nullptr); break;
        case PropertyType::kFloat4: id = PROPERTY_MANAGER.get_or_create_raw(name.c_str(), sizeof(XMFLOAT4) * len, nullptr); break;
        case PropertyType::kFloat4x4: id = PROPERTY_MANAGER.get_or_create_raw(name.c_str(), sizeof(XMFLOAT4X4) * len, nullptr); break;
        default: assert(!"Unknown type!"); break;
        }
      }
    } else {
      id = -1;
    }
  }

  union DefaultValue {
    int _int;
    float _float[16];
  };

  PropertyId id;
  DefaultValue default_value;
};

struct ResourceViewParam : public ParamBase {
  ResourceViewParam(const std::string &name, PropertyType::Enum type, PropertySource::Enum source, const std::string &friendly_name) 
    : ParamBase(name, type, source)
    , friendly_name(friendly_name)
  {
  }
  std::string friendly_name;
};


struct ShaderTemplate {
  friend TechniqueParser;

  ShaderTemplate(ShaderType::Enum type) : _type(type) {}

  CBufferParam *find_cbuffer_param(const std::string &name) {
    return find_by_name(name, _cbuffer_params);
  }

  ResourceViewParam *find_resource_view(const std::string &name) {
    return find_by_name(name, _resource_view_params);
  }

  template<class T>
  T* find_by_name(const std::string &name, std::vector<T> &v) {
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i].name == name)
        return &v[i];
    }
    return NULL;
  }

  ShaderType::Enum _type;
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
  Shader *vertex_shader(int flags) const { return _vertex_shaders[flags & _vs_flag_mask]; }
  Shader *pixel_shader(int flags) const { return _pixel_shaders[flags & _ps_flag_mask]; }

  //vector<CBuffer> &get_cbuffers() { return _constant_buffers; }

  //void get_render_objects(RenderObjects *obj, int vs_flags, int ps_flags);

  GraphicsObjectHandle rasterizer_state() const { return _rasterizer_state; }
  GraphicsObjectHandle blend_state() const { return _blend_state; }
  GraphicsObjectHandle depth_stencil_state() const { return _depth_stencil_state; }
  //GraphicsObjectHandle sampler_state(const char *name) const;

  GraphicsObjectHandle vb() const { return _vb; }
  GraphicsObjectHandle ib() const { return _ib; }
  int vertex_size() const { return _vertex_size; }
  DXGI_FORMAT index_format() const { return _index_format; }
  int index_count() const { return (int)_indices.size(); }

  bool init(ResourceInterface *res);
  bool reload_shaders();
  bool create_shaders(ResourceInterface *res, ShaderTemplate *shader_template);

  bool is_valid() const { return _valid; }
  const std::string &error_msg() const { return _error_msg; }

  const std::string &get_default_sampler_state() const { return _default_sampler_state; }
  const TechniqueRenderData &render_data() const { return _render_data; }

  void fill_samplers(const SparseProperty& input, std::vector<GraphicsObjectHandle> *out) const;
  void fill_cbuffer(CBuffer *cbuffer) const;
  void fill_resource_views(const SparseUnknown &props, std::vector<GraphicsObjectHandle> *out) const;

  const std::vector<uint8> &cbuffer() const { return _cbuffer_staged; }
  GraphicsObjectHandle cbuffer_handle();

private:

  void prepare_cbuffers();
  void prepare_textures();

  void add_error_msg(const char *fmt, ...);
  bool compile_shader(ShaderType::Enum type, const char *entry_point, const char *src, const char *obj, const std::vector<std::string> &flags);
  bool do_reflection(const std::vector<char> &text, Shader *shader, ShaderTemplate *shader_template, const std::vector<char> &obj);

  //bool parse_input_layout(ID3D11ShaderReflection* reflector, const D3D11_SHADER_DESC &shader_desc, void *buf, size_t len);

  std::vector<CBufferVariable> _cbuffer_vars;
  std::vector<uint8> _cbuffer_staged;

  string _name;
  // we have multiple version of the shaders, one for each permutation of the compilation flags
  std::vector<Shader *> _vertex_shaders;
  std::vector<Shader *> _pixel_shaders;

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
  std::vector<std::pair<PropertyId, GraphicsObjectHandle> > _sampler_states;
  GraphicsObjectHandle _blend_state;
  GraphicsObjectHandle _depth_stencil_state;

  //GraphicsObjectHandle _ordered_sampler_states[MAX_SAMPLERS];
  //int _first_sampler;
  //int _num_valid_samplers;


  CD3D11_RASTERIZER_DESC _rasterizer_desc;
  std::vector<std::pair<std::string, CD3D11_SAMPLER_DESC>> _sampler_descs;
  CD3D11_BLEND_DESC _blend_desc;
  CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;

  int _vs_flag_mask;
  int _ps_flag_mask;
  ShaderTemplate *_vs_shader_template;
  ShaderTemplate *_ps_shader_template;

  vector<Material *> _materials;

  std::string _error_msg;
  bool _valid;

  TechniqueRenderData _render_data;
};
