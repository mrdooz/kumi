#pragma once

namespace technique_parser_details {
	enum Symbol;
	class Trie;
}

// my take on a recursive descent parser
class TechniqueParser {
public:
	TechniqueParser();
	~TechniqueParser();
	bool parse_main(const char *start, const char *end, Technique *technique);
private:

	const char *next_symbol(const char *start, const char *end, technique_parser_details::Symbol *symbol);
	const char *next_int(const char *start, const char *end, int *out);
	const char *next_identifier(const char *start, const char *end, string *id);
	bool expect(technique_parser_details::Symbol symbol, const char **start, const char *end);
	const char *string_until(const char *start, const char *end, char delim, string *res);
	void parse_value(const string &value, ShaderParam *param);
	const char *parse_param(const char *start, const char *end, ShaderParam *param);
	const char *parse_params(const char *start, const char *end, vector<ShaderParam> *params);
	const char *parse_shader(const char *start, const char *end, Shader *shader);
	const char *parse_rasterizer_desc(const char *start, const char *end, D3D11_RASTERIZER_DESC *desc);
	const char *parse_sampler_desc(const char *start, const char *end, D3D11_SAMPLER_DESC *desc);
	const char *parse_render_target(const char *start, const char *end, D3D11_RENDER_TARGET_BLEND_DESC *desc);
	const char *parse_blend_desc(const char *start, const char *end, D3D11_BLEND_DESC *desc);
	const char *parse_depth_stencil_desc(const char *start, const char *end, D3D11_DEPTH_STENCIL_DESC *desc);
	const char *parse_technique(const char *start, const char *end, Technique *technique);
	const char *skip_whitespace(const char **start, const char *end);
	const char *skip_whitespace(const char *start, const char *end);
	const char *scope_extents(const char *start, const char *end, char scope_open, char scope_close);

	technique_parser_details::Trie *_symbol_trie;
};
