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

struct ResourceView {
  ResourceView() : used(false), source(PropertySource::kUnknown), class_id(~0) {}
  bool used;
  PropertySource::Enum source;
  PropertyId class_id;
};

typedef std::array<ResourceView, MAX_TEXTURES> ResourceViewArray;

class Shader {
  friend class TechniqueParser;
  friend class Technique;
public:

  Shader(ShaderType::Enum type);

  ShaderType::Enum type() const { return _type; }

  const std::string &source_filename() const { return _source_filename; }
  GraphicsObjectHandle handle() const { return _handle; }

  CBuffer &mesh_cbuffer() { return _mesh_cbuffer; }
  CBuffer &material_cbuffer() { return _material_cbuffer; }
  CBuffer &system_cbuffer() { return _system_cbuffer; }
  CBuffer &instance_cbuffer() { return _instance_cbuffer; }
  const std::vector<CBuffer *> cbuffers() const { return _cbuffers; }
  const SamplerArray &samplers() { return _samplers; }
  const ResourceViewArray &resource_views() { return _resource_views; }

  bool on_loaded();

  void set_cbuffer_slot(PropertySource::Enum src, int slot);

  GraphicsObjectHandle input_layout() const { return _input_layout; }

private:

  CBuffer _mesh_cbuffer;
  CBuffer _material_cbuffer;
  CBuffer _system_cbuffer;
  CBuffer _instance_cbuffer;
  std::vector<CBuffer *> _cbuffers;
  std::string _source_filename;
#if _DEBUG
  std::string _entry_point;
  std::string _obj_filename;
  std::vector<std::string> _flags;
#endif
  GraphicsObjectHandle _handle;
  ShaderType::Enum _type;

  GraphicsObjectHandle _input_layout;

  ResourceViews _resource_views2;
  ResourceViewArray _resource_views;

  SamplerArray _samplers;
};
