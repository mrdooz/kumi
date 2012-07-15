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

  std::vector<CBuffer> &cbuffers() { return _cbuffers; }
  const SamplerArray &samplers() { return _samplers; }
  const ResourceViewArray &resource_views() { return _resource_views; }

  bool on_loaded();

private:

  std::vector<CBuffer> _cbuffers;
  std::string _source_filename;
#if _DEBUG
  std::string _entry_point;
  std::string _obj_filename;
  std::vector<std::string> _flags;
#endif
  GraphicsObjectHandle _handle;
  ShaderType::Enum _type;

  ResourceViews _resource_views2;
  ResourceViewArray _resource_views;

  SamplerArray _samplers;
};
