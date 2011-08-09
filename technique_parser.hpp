#pragma once

namespace technique_parser_details {
	enum Symbol;
	class Trie;
}

struct Scope;

// my take on a recursive descent parser
class TechniqueParser {
public:
	TechniqueParser();
	~TechniqueParser();
	bool parse_main(const char *start, const char *end, Technique *technique);
private:
	bool parse_inner(Scope *scope, Technique *technique);
	bool parse_technique(Scope *scope, Technique *technique);
	bool parse_shader(Scope *scope, Shader *shader);
	bool parse_params(Scope *scope, vector<ShaderParam> *params);
	bool parse_param(Scope *scope, ShaderParam *param);
	bool parse_rasterizer_desc(Scope *scope, D3D11_RASTERIZER_DESC *desc);
	bool parse_sampler_desc(Scope *scope, D3D11_SAMPLER_DESC *desc);
	bool parse_render_target(Scope *scope, D3D11_RENDER_TARGET_BLEND_DESC *desc);
	bool parse_blend_desc(Scope *scope, D3D11_BLEND_DESC *desc);
	bool parse_depth_stencil_desc(Scope *scope, D3D11_DEPTH_STENCIL_DESC *desc);

	technique_parser_details::Trie *_symbol_trie;
	hash_map<technique_parser_details::Symbol, string > _symbol_to_string;
};
