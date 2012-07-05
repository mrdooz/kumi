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

typedef SparseResource<UnknownResource> SparseUnknown;

class Shader {
  friend class TechniqueParser;
  friend class Technique;
public:

  Shader(ShaderType::Enum type);

  ShaderType::Enum type() const { return _type; }
  bool is_valid() const { return _valid; }

  const std::string &source_filename() const { return _source_filename; }
  GraphicsObjectHandle handle() const { return _handle; }

  void prepare_cbuffers();

  std::vector<CBuffer> &cbuffers() { return _cbuffers; }
  const std::vector<GraphicsObjectHandle> &samplers() { return _samplers; }
  const SparseUnknown &resource_views() const { return _resource_views; }

  bool on_loaded();

private:

  std::vector<CBuffer> _cbuffers;
  bool _valid;
  std::string _source_filename;
#if _DEBUG
  std::string _entry_point;
  std::string _obj_filename;
  std::vector<std::string> _flags;
#endif
  GraphicsObjectHandle _handle;
  ShaderType::Enum _type;

  ResourceViews _resource_views2;
  SparseUnknown _resource_views;
  //SparseProperty _sampler_states;

  std::vector<GraphicsObjectHandle> _samplers;
};
