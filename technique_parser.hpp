#pragma once
#include "graphics_object_handle.hpp"

class Technique;
class Shader;
struct ShaderTemplate;

namespace technique_parser_details {
  enum Symbol;
  class Trie;
}

struct Scope;
class Material;

struct TechniqueFile {

  std::map<std::string, GraphicsObjectHandle> rasterizer_states;
  std::map<std::string, GraphicsObjectHandle> sampler_states;
  std::map<std::string, GraphicsObjectHandle> depth_stencil_states;
  std::map<std::string, GraphicsObjectHandle> blend_states;

  std::vector<Technique *> techniques;
  std::vector<Material *> materials;
};

class TechniqueParser {
public:
  TechniqueParser(const std::string &filename, TechniqueFile *result);
  ~TechniqueParser();
  bool parse();
private:
  friend struct Scope;
  void parse_technique(Scope *scope, Technique *technique);
  void parse_shader_template(Scope *scope, Technique *technique, ShaderTemplate *shader);
  void parse_params(const std::vector<std::vector<std::string>> &items, Technique *technique, Shader *shader);
  void parse_param(const std::vector<std::string> &param, Technique *technique, ShaderTemplate *shader);
  void parse_material(Scope *scope, Material *material);
  void parse_rasterizer_desc(Scope *scope, CD3D11_RASTERIZER_DESC *desc);
  void parse_sampler_desc(Scope *scope, CD3D11_SAMPLER_DESC *desc);
  void parse_render_target(Scope *scope, D3D11_RENDER_TARGET_BLEND_DESC *desc);
  void parse_blend_desc(Scope *scope, CD3D11_BLEND_DESC *desc);
  void parse_depth_stencil_desc(Scope *scope, CD3D11_DEPTH_STENCIL_DESC *desc);
  void parse_vertices(Scope *scope, Technique *technique);
  void parse_indices(Scope *scope, Technique *technique);

  std::unique_ptr<technique_parser_details::Trie> _symbol_trie;
  std::unordered_map<technique_parser_details::Symbol, std::string > _symbol_to_string;

  std::string _filename;
  TechniqueFile *_result;
};
