#pragma once

#include "utils.hpp"
#include "property.hpp"
#include "graphics_object_handle.hpp"
#include "graphics_submit.hpp"
#include "property_manager.hpp"
#include "shader.hpp"

struct ResourceInterface;

class Material;
class Technique;

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
      int array_size = max(1, HIWORD(type));
      std::string qualified_name = PropertySource::qualify_name(name, source);
      int len = PropertyType::len((PropertyType::Enum)LOWORD(type));
      id = PROPERTY_MANAGER.get_or_create_raw(qualified_name.c_str(), array_size * len, nullptr);
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

  const std::string &name() const { return _name; }
  void init_from_parent(const Technique *parent);

  GraphicsObjectHandle input_layout() const { return _input_layout; }
  Shader *vertex_shader(int flags) const;
  Shader *pixel_shader(int flags) const;

  GraphicsObjectHandle rasterizer_state() const { return _rasterizer_state; }
  GraphicsObjectHandle blend_state() const { return _blend_state; }
  GraphicsObjectHandle depth_stencil_state() const { return _depth_stencil_state; }

  GraphicsObjectHandle vb() const { return _vb; }
  GraphicsObjectHandle ib() const { return _ib; }
  int vertex_size() const { return _vertex_size; }
  DXGI_FORMAT index_format() const { return _index_format; }
  int index_count() const { return _index_count; }

  bool init();
  bool reload_shaders();
  bool create_shaders(ShaderTemplate *shader_template);

  bool is_valid() const { return _valid; }
  const std::string &error_msg() const { return _error_msg; }

  void fill_cbuffer(CBuffer *cbuffer) const;
private:

  void prepare_cbuffers();

  void add_error_msg(const char *fmt, ...);
  bool compile_shader(ShaderType::Enum type, const char *entry_point, const char *src, const char *obj, const std::vector<std::string> &flags);
  bool do_reflection(const std::vector<char> &text, Shader *shader, ShaderTemplate *shader_template, const std::vector<char> &obj);

  std::vector<CBufferVariable> _cbuffer_vars;

  std::string _name;
  // we have multiple version of the shaders, one for each permutation of the compilation flags
  std::vector<Shader *> _vertex_shaders;
  std::vector<Shader *> _pixel_shaders;

  GraphicsObjectHandle _input_layout;

  int _vertex_size;
  DXGI_FORMAT _index_format;
  int _index_count;
  GraphicsObjectHandle _vb;
  GraphicsObjectHandle _ib;

  GraphicsObjectHandle _rasterizer_state;
  GraphicsObjectHandle _blend_state;
  GraphicsObjectHandle _depth_stencil_state;

  int _vs_flag_mask;
  int _ps_flag_mask;
  std::shared_ptr<ShaderTemplate> _vs_shader_template;
  std::shared_ptr<ShaderTemplate> _ps_shader_template;

  std::vector<Material *> _materials;

  std::string _error_msg;
  bool _valid;
};
