#pragma once
#include "property.hpp"
#include "property_manager.hpp"
#include "graphics_submit.hpp"

struct GraphicsInterface;

struct ParamBase {
  ParamBase(const std::string &name, PropertyType::Enum type, PropertySource::Enum source) 
    : name(name), type(type), source(source), used(false) {}
  std::string name;
  PropertyType::Enum type;
  PropertySource::Enum source;
  bool used;
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

  int start_offset;  // offset in cbuffer
  int size;
  PropertyId id;
  DefaultValue default_value;
};
/*
struct SamplerParam : public ParamBase {
  SamplerParam(const std::string &name, PropertyType::Enum type, PropertySource::Enum source) : ParamBase(name, type, source) {}
  int bind_point;
};
*/
struct ResourceViewParam : public ParamBase {
  ResourceViewParam(const std::string &name, PropertyType::Enum type, PropertySource::Enum source, const std::string &friendly_name) 
    : ParamBase(name, type, source)
    , friendly_name(friendly_name)
    , bind_point(INT_MAX)
  {
  }
  std::string friendly_name;
  int bind_point;
};

class Shader {
public:
  enum Type {
    kVertexShader,
    kPixelShader,
    kGeometryShader,
  };

  Shader(const std::string &entry_point) : _entry_point(entry_point), _valid(false) {}
  virtual ~Shader() {}
/*
  CBufferParam *find_cbuffer_param(const char *name) {
    return find_by_name(name, _cbuffer_params);
  }
*/
  ResourceViewParam *find_resource_view_param(const char *name) {
    return find_by_name(name, _resource_view_params);
  }

  template<class T>
  T* find_by_name(const char *name, std::vector<T> &v) {
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i].name == name)
        return &v[i];
    }
    return NULL;
  }

  virtual void set_buffers() = 0;
  virtual Type type() const = 0;
  bool is_valid() const { return _valid; }
  void set_source_filename(const std::string &filename) { _source_filename = filename; }
  const std::string &source_filename() const { return _source_filename; }
  void set_entry_point(const std::string &entry_point) { _entry_point = entry_point; }
  const std::string &entry_point() const { return _entry_point; }
  void set_obj_filename(const std::string &filename) { _obj_filename = filename; }
  const std::string &obj_filename() const { return _obj_filename; }

  //std::vector<CBufferParam> &cbuffer_params() { return _cbuffer_params; }
  std::vector<ResourceViewParam> &resource_view_params() { return _resource_view_params; }

  void set_handle(GraphicsObjectHandle handle) { _handle = handle; }
  GraphicsObjectHandle handle() const { return _handle; }
  void validate() { _valid = true; } // tihi

  std::string id() const;

  void prune_unused_parameters();

private:

  bool _valid;
  std::string _source_filename;
  std::string _obj_filename;
  std::string _entry_point;
  //std::vector<CBufferParam> _cbuffer_params;
  std::vector<ResourceViewParam> _resource_view_params;
  GraphicsObjectHandle _handle;
};

struct VertexShader : public Shader{
  VertexShader() : Shader("vs_main") {}
  virtual void set_buffers() {
  }
  virtual Type type() const { return kVertexShader; }
};

struct PixelShader : public Shader {
  PixelShader() : Shader("ps_main") {}
  virtual void set_buffers() {
  }
  virtual Type type() const { return kPixelShader; }
};

/*
struct CBuffer {
  CBuffer(const std::string &name, int size, GraphicsObjectHandle handle) 
    : name(name), handle(handle) 
  {
    staging.resize(size);
  }
  std::string name;
  std::vector<uint8> staging;
  GraphicsObjectHandle handle;
};
*/