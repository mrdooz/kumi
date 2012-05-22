#pragma once

struct GraphicsInterface;
class Technique;
class Shader;

using std::vector;
using std::string;

namespace technique_parser_details {
	enum Symbol;
	class Trie;
}

struct Scope;
class Material;

// my take on a recursive descent parser
class TechniqueParser {
public:
	TechniqueParser();
	~TechniqueParser();
	bool parse(GraphicsInterface *graphics, const char *start, const char *end, vector<Technique *> *techniques, vector<Material *> *materials);
private:
	friend struct Scope;
	void parse_technique(GraphicsInterface *graphics, Scope *scope, Technique *technique);
	void parse_shader(Scope *scope, Shader *shader);
	void parse_params(const vector<vector<string>> &items, Shader *shader);
	void parse_param(const vector<string> &param, Shader *shader);
	void parse_material(Scope *scope, Material *material);
	void parse_rasterizer_desc(Scope *scope, D3D11_RASTERIZER_DESC *desc);
	void parse_sampler_desc(Scope *scope, D3D11_SAMPLER_DESC *desc);
	void parse_render_target(Scope *scope, D3D11_RENDER_TARGET_BLEND_DESC *desc);
	void parse_blend_desc(Scope *scope, D3D11_BLEND_DESC *desc);
	void parse_depth_stencil_desc(Scope *scope, D3D11_DEPTH_STENCIL_DESC *desc);
	void parse_vertices(GraphicsInterface *graphics, Scope *scope, Technique *technique);
	void parse_indices(GraphicsInterface *graphics, Scope *scope, Technique *technique);

	std::unique_ptr<technique_parser_details::Trie> _symbol_trie;
	std::unordered_map<technique_parser_details::Symbol, string > _symbol_to_string;
};
