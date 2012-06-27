#pragma once
#include "property.hpp"
#include "property_manager.hpp"
#include "graphics_submit.hpp"

namespace ShaderType {
  enum Enum {
    kVertexShader,
    kPixelShader,
    kGeometryShader,
  };
}

// worse names evar!
template<typename T>
struct SparseResource {
  SparseResource() : first(INT_MAX), count(0) {}
  int first;
  int count;
  std::vector<T> res;
};

struct UnknownResource {
  PropertySource::Enum source;
  char data[max(sizeof(GraphicsObjectHandle), sizeof(PropertyId))];
  const GraphicsObjectHandle *goh() const { return (GraphicsObjectHandle *)&data; }
  GraphicsObjectHandle *goh() { return (GraphicsObjectHandle *)&data; }
  const PropertyId *pid() const { return (PropertyId *)&data; }
  PropertyId *pid() { return (PropertyId *)&data; }
};

typedef SparseResource<PropertyId> SparseProperty;
typedef SparseResource<UnknownResource> SparseUnknown;

class Shader {
  friend class TechniqueParser;
  friend class Technique;
public:

  Shader(ShaderType::Enum type) : _type(type), _valid(false) {}
  virtual ~Shader() {}

  ShaderType::Enum type() const { return _type; }
  bool is_valid() const { return _valid; }
  void set_source_filename(const std::string &filename) { _source_filename = filename; }
  const std::string &source_filename() const { return _source_filename; }
  void set_entry_point(const std::string &entry_point) { _entry_point = entry_point; }
  const std::string &entry_point() const { return _entry_point; }
  void set_obj_filename(const std::string &filename) { _obj_filename = filename; }
  const std::string &obj_filename() const { return _obj_filename; }

  void set_handle(GraphicsObjectHandle handle) { _handle = handle; }
  GraphicsObjectHandle handle() const { return _handle; }

  std::string id() const;

  void prepare_cbuffers();

  std::vector<CBuffer> &get_cbuffers() { return _cbuffers; }

  bool on_loaded();

  const SparseProperty &samplers() const;
  const SparseUnknown &resource_views() const;

private:
  //void prune_unused_parameters();

  std::vector<CBuffer> _cbuffers;
  bool _valid;
#if _DEBUG
  std::string _source_filename;
  std::string _obj_filename;
  std::string _entry_point;
  std::vector<std::string> _flags;
#endif
  //std::vector<CBufferParam> _cbuffer_params;
  //std::vector<ResourceViewParam> _resource_view_params;
  GraphicsObjectHandle _handle;
  ShaderType::Enum _type;

  //std::vector<std::string> _resource_view_names;
  //std::vector<std::string> _sampler_names;

  SparseUnknown _resource_views;
  SparseProperty _sampler_states;
};
/*
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
*/
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
