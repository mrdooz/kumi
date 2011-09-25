#include "stdafx.h"
#include "technique.hpp"
#include "technique_parser.hpp"
#include "property_manager.hpp"
#include "string_utils.hpp"
#include "file_utils.hpp"
#include "dx_utils.hpp"
#include "material_manager.hpp"
#include <hash_set>

using namespace std;
using namespace boost::assign;

namespace technique_parser_details {

enum Symbol {
	kSymInvalid = -1,
	kSymUnknown,
	kSymSingleLineComment,
	kSymMultiLineCommentStart,
	kSymMultiLineCommentEnd,
	kSymListOpen,
	kSymListClose,
	kSymBlockOpen,
	kSymBlockClose,
	kSymSemicolon,
	kSymComma,
	kSymEquals,
	kSymParenOpen,
	kSymParenClose,

	kSymTechnique,
	kSymId,
	kSymVertexShader,
	kSymPixelShader,
		kSymFile,
		kSymEntryPoint,
		kSymParams,

	kSymVertices,
		kSymFormat,
		kSymData,
	kSymIndices,

	kSymMaterial,

	kSymFloat,
	kSymFloat2,
	kSymFloat3,
	kSymFloat4,
	kSymFloat4x4,
	kSymTexture2d,
	kSymSampler,
	kSymInt,
	kSymParamDefault,

	kSymRasterizerDesc,
		kSymFillMode,
		kSymCullMode,
		kSymFrontCounterClockwise,
		kSymDepthBias,
		kSymDepthBiasClamp,
		kSymSlopeScaledDepthBias,
		kSymDepthClipEnable,
		kSymScissorEnable,
		kSymMultisampleEnable,
		kSymAntialiasedLineEnable,

	kSymSamplerDesc,
		kSymFilter,
		kSymAddressU,
		kSymAddressV,
		kSymAddressW,
		kSymMipLODBias,
		kSymMaxAnisotropy,
		kSymComparisonFunc,
		kSymBorderColor,
		kSymMinLOD,
		kSymMaxLOD,

	kSymDepthStencilDesc,
		kSymDepthEnable,

	kSymBlendDesc,
		kSymAlphaToCoverageEnable,
		kSymIndependentBlendEnable,
		kSymRenderTarget,
			kSymBlendEnable,
			kSymSrcBlend,
			kSymDestBlend,
			kSymBlendOp,
			kSymSrcBlendAlpha,
			kSymDestBlendAlpha,
			kSymBlendOpAlpha,
			kSymRenderTargetWriteMask,
};

struct {
	Symbol symbol;
	const char *str;
} g_symbols[] = {
	{ kSymSingleLineComment, "//" },
	{ kSymMultiLineCommentStart, "/*" },
	{ kSymListOpen, "[" },
	{ kSymListClose, "]" },
	{ kSymBlockOpen, "{" },
	{ kSymBlockClose, "}" },
	{ kSymSemicolon, ";" },
	{ kSymComma, "," },
	{ kSymEquals, "=" },
	{ kSymParenOpen, "(" },
	{ kSymParenClose, ")" },

	{ kSymTechnique, "technique" },

	{ kSymVertexShader, "vertex_shader" },
	{ kSymPixelShader, "pixel_shader" },
		{ kSymFile, "file" },
		{ kSymEntryPoint, "entry_point" },
		{ kSymParams, "params" },

	{ kSymMaterial, "material" },

	{ kSymVertices, "vertices" },
	{ kSymFormat, "format" },
	{ kSymData, "data" },
	{ kSymIndices, "indices" },

	// params
	{ kSymFloat, "float" },
	{ kSymFloat, "float1" },
	{ kSymFloat2, "float2" },
	{ kSymFloat3, "float3" },
	{ kSymFloat4, "float4" },
	{ kSymFloat4x4, "float4x4" },
	{ kSymInt, "int" },
	{ kSymTexture2d, "texture2d" },
	{ kSymSampler, "sampler" },
	{ kSymParamDefault, "default" },

	// descs
	{ kSymRasterizerDesc, "rasterizer_desc" },
		{ kSymFillMode, "fill_mode" },
		{ kSymCullMode, "cull_mode" },
		{ kSymFrontCounterClockwise, "front_counter_clockwise" },
		{ kSymDepthBias, "depth_bias" },
		{ kSymDepthBias, "depth_bias_clamp" },
		{ kSymSlopeScaledDepthBias, "slope_scaled_depth_bias" },
		{ kSymDepthClipEnable, "depth_clip_enable" },
		{ kSymScissorEnable, "scissor_enable" },
		{ kSymMultisampleEnable, "multisample_enable" },
		{ kSymAntialiasedLineEnable, "antialiased_line_enable" },

	{ kSymSamplerDesc, "sampler_desc" },
		{ kSymFilter, "filter" },
		{ kSymAddressU, "address_u" },
		{ kSymAddressV, "address_v" },
		{ kSymAddressW, "address_w" },
		{ kSymMipLODBias, "mip_lod_bias" },
		{ kSymMaxAnisotropy, "max_anisotropy" },
		{ kSymComparisonFunc, "comparison_func" },
		{ kSymBorderColor, "border_color" },
		{ kSymMinLOD, "min_lod" },
		{ kSymMaxLOD, "max_lod" },

	{ kSymDepthStencilDesc, "depth_stencil_desc" },
		{ kSymDepthEnable, "depth_enable" },

	{ kSymBlendDesc, "blend_desc" },
		{ kSymAlphaToCoverageEnable, "alpha_to_coverage_enable" },
		{ kSymIndependentBlendEnable, "independent_blend_enable" },
		{ kSymRenderTarget, "render_target" },
			{ kSymBlendEnable, "blend_enable" },
			{ kSymSrcBlend, "src_blend" },
			{ kSymDestBlend, "dest_blend" },
			{ kSymBlendOp, "blend_op" },
			{ kSymSrcBlendAlpha, "src_blend_alpha" },
			{ kSymDestBlendAlpha, "dest_blend_alpha" },
			{ kSymBlendOpAlpha, "blend_op_alpha" },
			{ kSymRenderTargetWriteMask, "render_target_write_mask" },
};

static auto valid_property_types = map_list_of
	(kSymFloat, PropertyType::kFloat)
	(kSymFloat2, PropertyType::kFloat2)
	(kSymFloat3, PropertyType::kFloat3)
	(kSymFloat4, PropertyType::kFloat4)
	(kSymFloat4x4, PropertyType::kFloat4x4)
	(kSymTexture2d, PropertyType::kTexture2d)
	(kSymSampler, PropertyType::kSampler);

class Trie {
public:
	void add_symbol(const char *str, int len, Symbol symbol) {
		Node *cur = &root;
		for (int i = 0; i < len; ++i) {
			char ch = str[i];
			if (!cur->children[ch]) {
				// create new node
				cur->children[ch] = new Node;
			}
			cur = cur->children[ch];
		}
		assert(cur->symbol == kSymUnknown);
		cur->symbol = symbol;
	}

	Symbol find_symbol(const char *str, int max_len, int *match_length) {
		// traverse the trie, and return the longest matching string
		Node *cur = &root;
		Node *last_match = NULL;
		int last_len = 0;
		int len = 0;

		while (cur && len < max_len) {
			if (cur->symbol != kSymUnknown) {
				last_match = cur;
				last_len = len;
			}
			cur = cur->children[str[len++]];
		}

		if (cur == NULL || cur->symbol == kSymUnknown) {
			if (last_match) {
				*match_length = last_len;
				return last_match->symbol;
			}
			return kSymUnknown;
		}

		*match_length = len;
		return cur->symbol;
	}

private:
	struct Node {
		Node() : symbol(kSymUnknown) {
			memset(children, 0, sizeof(children));
		}
		~Node() {
			for (int i = 0; i < 256; ++i)
				SAFE_DELETE(children[i]);
		}
		Symbol symbol;
		Node *children[256];
	};

	Node root;
};

}

static bool parse_value(const string &value, PropertyType::Enum type, float *out) {
	const char *v = value.c_str();
	switch (type) {
	case PropertyType::kFloat:
		return (sscanf(v, "%f", &out[0]) == 1);
	case PropertyType::kFloat2:
		return sscanf(v, "%f %f", &out[0], &out[1]) == 2 || sscanf(v, "%f, %f", &out[0], &out[1]) == 2;
	case PropertyType::kFloat3:
		return sscanf(v, "%f %f %f", &out[0], &out[1], &out[2]) == 3 || sscanf(v, "%f, %f, %f", &out[0], &out[1], &out[2]) == 3;
	case PropertyType::kFloat4:
		return sscanf(v, "%f %f %f %f", &out[0], &out[1], &out[2], &out[3]) == 4 || sscanf(v, "%f, %f, %f, %f", &out[0], &out[1], &out[2], &out[3]) == 4;
	default:
		return false;
	}
}

using namespace technique_parser_details;

TechniqueParser::TechniqueParser() : _symbol_trie(new Trie()) {
	for (int i = 0; i < ELEMS_IN_ARRAY(g_symbols); ++i) {
		_symbol_trie->add_symbol(g_symbols[i].str, strlen(g_symbols[i].str), g_symbols[i].symbol);
		_symbol_to_string[g_symbols[i].symbol] = g_symbols[i].str;
	}
}

TechniqueParser::~TechniqueParser() {
	SAFE_DELETE(_symbol_trie);
}

struct Scope {
	// start and end point to the first and last character of the scope (inclusive)
	Scope() : _symbol_trie(nullptr), _org_start(nullptr), _start(nullptr), _end(nullptr) {}
	Scope(Trie *symbol_trie, const char *start, const char *end) : _symbol_trie(symbol_trie), _org_start(start), _start(start), _end(end) {}
	bool is_valid() const { return _start != nullptr; }
	bool end() const { return _start >= _end; }
	bool advance(const Scope &inner) {
		// advance the scope to point to the character after the inner scope
		if (inner._start < _start || inner._end >= _end)
			return false;
		_start = inner._end + 1;
		return true;
	}

	bool consume_if(Symbol symbol) {
		if (peek() == symbol) {
			next_symbol();
			return true;
		}
		return false;
	}

	Symbol consume_in(const vector<Symbol> &symbols) {
		Symbol tmp = peek();
		for (size_t i = 0; i < symbols.size(); ++i) {
			if (symbols[i] == tmp)
				return next_symbol();
		}
		return kSymInvalid;
	}

	bool consume_in(const vector<Symbol> &symbols, Symbol *out) {
		Symbol tmp = peek();
		for (size_t i = 0; i < symbols.size(); ++i) {
			if (symbols[i] == tmp) {
				*out = next_symbol();
				return true;
			}
		}
		return false;
	}

	Symbol next_symbol() {
		int len = 0;
		Symbol s = _symbol_trie->find_symbol(_start, _end - _start + 1, &len);

		_start += len;
		skip_whitespace();
		return s;
	}

	void skip_whitespace() {
		bool removed;
		do {
			removed = false;
			while (_start <= _end && isspace((uint8_t)*_start))
				++_start;

			if (_start == _end)
				return;

			int len = 0;
			Symbol s = _symbol_trie->find_symbol(_start, _end - _start + 1, &len);

			// skip comments
			if (s == kSymSingleLineComment) {
				removed = true;
				// skip until CR-LF
				_start += len;
				while (_start <= _end && *_start != '\r')
					++_start;
				if (_start < _end && _start[1] == '\n')
					++_start;


			} else if (s == kSymMultiLineCommentStart) {
				removed = true;
				_start += len;
				while (_start < _end && _start[0] != '*' && _start[1] != '/')
					++_start;
				if (_start == _end)
					return;
				_start += 2;
			}

		} while(removed);
	}

	int next_int() {
		bool sign = *_start == '-' || *_start == '+';
		int res = atoi(_start);
		_start += (res != 0 ? (int)log10(abs((double)res)) : 0) + 1 + (sign ? 1 : 0);
		skip_whitespace();
		return res;
	}

	float next_float() {
		float r = (float)next_int();
		float frac = 0;
		if (*_start == '.') {
			++_start;
			float mul = 0.1f;
			while (_start <= _end && isdigit((byte)*_start)) {
				frac += *_start - '0' * mul;
				mul /= 10;
				_start++;
			}
		}
		skip_whitespace();
		return r + frac;
	}

	string next_identifier() {
		const char *tmp = _start;
		while (tmp <= _end && (isalnum((uint8_t)*tmp) || *tmp == '_'))
			++tmp;

		string res(_start, tmp - _start);
		_start = tmp;
		skip_whitespace();
		return res;
	}

	bool expect(Symbol symbol) {
		return next_symbol() == symbol;
	}

	Symbol peek() {
		const char *tmp = _start;
		Symbol sym = next_symbol();
		_start = tmp;
		return sym;
	}

	string string_until(char delim) {
		skip_whitespace();
		const char *tmp = _start;
		while (_start <= _end && *_start != delim)
			++_start;

		return string(tmp, _start - tmp);
	}

	void log_error(const char *expected_symbol) {
		// an error occured at the current position, so work backwards to figure out the line #
		int line = 1;
		const char *tmp = _start;
		while (tmp >= _org_start) {
			// CR-LF
			if (*tmp == '\n' && tmp > _org_start && tmp[-1] == '\r')
				++line;
			--tmp;
		}

		LOG_ERROR(to_string("Error on line: %d. Looking for \"%s\", found \"%s\"", 
			line, 
			expected_symbol,
			string(_start, min(_end - _start + 1, 20)).c_str()).c_str());
	}

	technique_parser_details::Trie *_symbol_trie;
	const char *_org_start;
	const char *_start;
	const char *_end;
};

#define EXPECT(scope, symbol)                                         \
	do {                                                                \
		if (scope->peek() != symbol) {                                    \
				scope->log_error(_symbol_to_string[symbol].c_str());          \
				return false;                                                 \
		} else {                                                          \
			scope->next_symbol();                                           \
		}                                                                 \
	} while(false)

bool TechniqueParser::parse_param(Scope *scope, Shader *shader) {

	static auto valid_sources = map_list_of
		("material", PropertySource::kMaterial)
		("system", PropertySource::kSystem)
		("user", PropertySource::kUser)
		("mesh", PropertySource::kMesh)
		("technique", PropertySource::kTechnique);

	// type, name and source are required. the default value is optional
	PropertyType::Enum type = lookup_default<Symbol, PropertyType::Enum>(scope->next_symbol(), valid_property_types, PropertyType::kUnknown);
	if (type == PropertyType::kUnknown)
		return false;

	string name = scope->next_identifier();
	PropertySource::Enum source = lookup_default<string, PropertySource::Enum>(scope->next_identifier(), valid_sources, PropertySource::kUnknown);
	if (source == PropertySource::kUnknown)
		return false;

	bool cbuffer_param = false;
	if (type == PropertyType::kSampler) {
		shader->sampler_params.push_back(SamplerParam(name, type, source));
	} else if (type == PropertyType::kTexture2d) {
		shader->resource_view_params.push_back(ResourceViewParam(name, type, source));
	} else {
		shader->cbuffer_params.push_back(CBufferParam(name, type, source));
		cbuffer_param = true;
	}

	EXPECT(scope, kSymSemicolon);

	// check for default value
	if (cbuffer_param && scope->consume_if(kSymParamDefault)) {
		string value = scope->string_until(';');
		EXPECT(scope, kSymSemicolon);
		parse_value(value, shader->cbuffer_params.back().type, &shader->cbuffer_params.back().default_value._float[0]);
	}

	return true;
}

bool TechniqueParser::parse_params(Scope *scope, Shader *shader) {
	while (true) {
		EXPECT(scope, kSymBlockOpen);
		if (!parse_param(scope, shader))
			return false;
		EXPECT(scope, kSymBlockClose);
		if (!scope->consume_if(kSymComma))
			return true;
	}
	return false;
}

bool TechniqueParser::parse_material(Scope *scope, Material *material) {

	static auto tmp = list_of(kSymFloat)(kSymFloat2)(kSymFloat3)(kSymFloat4);

	Symbol type;
	while (scope->consume_in(tmp, &type)) {
		PropertyType::Enum prop_type = lookup_default<Symbol, PropertyType::Enum>(type, valid_property_types, PropertyType::kUnknown);
		string name = scope->next_identifier();
		EXPECT(scope, kSymEquals);
		string value = scope->string_until(';');
		float out[16];
		if (!parse_value(value, prop_type, out))
			return false;
		EXPECT(scope, kSymSemicolon);
		switch (prop_type) {
		case PropertyType::kFloat:
			material->properties.push_back(MaterialProperty(name, out[0]));
			break;
		case PropertyType::kFloat4:
			material->properties.push_back(MaterialProperty(name, XMFLOAT4(out[0], out[1], out[2], out[3])));
			break;
		default:
			return false;
		}
	}
	return true;
}

bool TechniqueParser::parse_shader(Scope *scope, Shader *shader) {

	const string &ext = shader->type() == Shader::kVertexShader ? ".vso" : ".pso";
	string entry_point = shader->type() == Shader::kVertexShader ? "vs_main" : "ps_main";
	while (true) {
		switch (scope->consume_in(list_of(kSymFile)(kSymEntryPoint)(kSymParams))) {

			case kSymFile: {
				shader->source_filename = scope->string_until(';');
				// Use the default entry point name until we get a real tag
				shader->obj_filename = strip_extension(shader->source_filename.c_str()) + "_" + entry_point + ext;
				EXPECT(scope, kSymSemicolon);
				break;
			}

			case kSymEntryPoint:
				entry_point = shader->entry_point = scope->string_until(';');
				// update the obj filename
				if (!shader->source_filename.empty())
					shader->obj_filename = strip_extension(shader->source_filename.c_str()) + "_" + entry_point + ext;
				EXPECT(scope, kSymSemicolon);
				break;

			case kSymParams:
				EXPECT(scope, kSymListOpen);
				parse_params(scope, shader);
				EXPECT(scope, kSymListClose);
				EXPECT(scope, kSymSemicolon);
				break;

			default:
				return true;
		}
	}
}

bool TechniqueParser::parse_depth_stencil_desc(Scope *scope, D3D11_DEPTH_STENCIL_DESC *desc) {

	while (!scope->end()) {
		Symbol symbol;
		if (scope->consume_in(list_of(kSymDepthEnable), &symbol)) {

			EXPECT(scope, kSymEquals);
			string value = scope->next_identifier();
			EXPECT(scope, kSymSemicolon);

			switch (symbol) {
			case kSymDepthEnable:
				lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->DepthEnable);
				break;
			}
		} else {
			return true;
		}
	}
	return false;
}

bool TechniqueParser::parse_sampler_desc(Scope *scope, D3D11_SAMPLER_DESC *desc) {

	static auto valid = list_of
		(kSymFilter)(kSymAddressU)(kSymAddressV)(kSymAddressW)
		(kSymMipLODBias)(kSymMaxAnisotropy)(kSymComparisonFunc)(kSymBorderColor)(kSymMinLOD)(kSymMaxLOD);

	static auto valid_filters = map_list_of
		("min_mag_mip_point", D3D11_FILTER_MIN_MAG_MIP_POINT)
		("min_mag_point_mip_liner", D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR)
		("min_point_mag_linear_mip_point", D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT)
		("min_linear_mag_mip_point", D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT)
		("min_linear_mag_point_mip_linear", D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR)
		("min_mag_linear_mip_point", D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT)
		("min_mag_mip_liner", D3D11_FILTER_MIN_MAG_MIP_LINEAR)
		("anisotropic", D3D11_FILTER_ANISOTROPIC)
		("comparison_min_mag_mip_point", D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT)
		("comparison_min_mag_point_mip_liner", D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR)
		("comparison_min_point_mag_linear_mip_point", D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT)
		("comparison_min_linear_mag_mip_point", D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT)
		("comparison_min_linear_mag_point_mip_linear", D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR)
		("comparison_min_mag_linear_mip_point", D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT)
		("comparison_min_mag_mip_liner", D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR)
		("comparison_anisotropic", D3D11_FILTER_COMPARISON_ANISOTROPIC);

	static auto valid_address = map_list_of
		("wrap", D3D11_TEXTURE_ADDRESS_WRAP)
		("mirror", D3D11_TEXTURE_ADDRESS_MIRROR)
		("clamp", D3D11_TEXTURE_ADDRESS_CLAMP)
		("border", D3D11_TEXTURE_ADDRESS_BORDER)
		("mirror_once", D3D11_TEXTURE_ADDRESS_MIRROR_ONCE);

	static auto valid_cmp_func = map_list_of
		("never", D3D11_COMPARISON_NEVER)
		("less", D3D11_COMPARISON_LESS)
		("equal", D3D11_COMPARISON_EQUAL)
		("less_equal", D3D11_COMPARISON_LESS_EQUAL)
		("greater", D3D11_COMPARISON_GREATER)
		("not_equal", D3D11_COMPARISON_NOT_EQUAL)
		("greater_equal", D3D11_COMPARISON_GREATER_EQUAL)
		("always", D3D11_COMPARISON_ALWAYS);

	while (!scope->end()) {
		Symbol symbol;
		if (scope->consume_in(valid, &symbol)) {
			EXPECT(scope, kSymEquals);
			string value = scope->next_identifier();
			EXPECT(scope, kSymSemicolon);

			switch (symbol) {
				case kSymFilter:
					lookup<string, D3D11_FILTER>(value, valid_filters, &desc->Filter);
					break;

				case kSymAddressU:
				case kSymAddressV:
				case kSymAddressW:
					lookup<string, D3D11_TEXTURE_ADDRESS_MODE>(value, valid_address, 
						symbol == kSymAddressU ? &desc->AddressU : symbol == kSymAddressV ? &desc->AddressV : &desc->AddressW);
					break;

				case kSymMipLODBias:
					sscanf(value.c_str(), "%f", &desc->MipLODBias);
					break;

				case kSymMaxAnisotropy:
					sscanf(value.c_str(), "%ud", &desc->MaxAnisotropy);
					break;

				case kSymComparisonFunc:
					lookup<string, D3D11_COMPARISON_FUNC>(value, valid_cmp_func, &desc->ComparisonFunc);
					break;

				case kSymBorderColor:
					sscanf(value.c_str(), "%f %f %f %f", 
						&desc->BorderColor[0], &desc->BorderColor[1], &desc->BorderColor[2], &desc->BorderColor[3]);
					break;

				case kSymMinLOD:
					sscanf(value.c_str(), "%f", &desc->MinLOD);
					break;

				case kSymMaxLOD:
					sscanf(value.c_str(), "%f", &desc->MaxLOD);
					break;
			}
		} else {
			return true;
		}
	}
	return false;
}

bool TechniqueParser::parse_render_target(Scope *scope, D3D11_RENDER_TARGET_BLEND_DESC *desc) {

	static auto valid = list_of
		(kSymBlendEnable)(kSymSrcBlend)(kSymDestBlend)(kSymSrcBlendAlpha)
		(kSymDestBlendAlpha)(kSymBlendOp)(kSymBlendOpAlpha)(kSymRenderTargetWriteMask);

	static auto valid_blend = map_list_of
		("zero", D3D11_BLEND_ZERO)
		("one", D3D11_BLEND_ONE)
		("src_color", D3D11_BLEND_SRC_COLOR)
		("inv_src_color", D3D11_BLEND_INV_SRC_COLOR)
		("src_alpha", D3D11_BLEND_SRC_ALPHA)
		("inv_src_alpha", D3D11_BLEND_INV_SRC_ALPHA)
		("dest_alpha", D3D11_BLEND_DEST_ALPHA)
		("inv_dest_alpha", D3D11_BLEND_INV_DEST_ALPHA)
		("dest_color", D3D11_BLEND_DEST_COLOR)
		("inv_dest_color", D3D11_BLEND_INV_DEST_COLOR)
		("src_alpha_sat", D3D11_BLEND_SRC_ALPHA_SAT)
		("blend_factor", D3D11_BLEND_BLEND_FACTOR)
		("inv_blend_factor", D3D11_BLEND_INV_BLEND_FACTOR)
		("src1_color", D3D11_BLEND_SRC1_COLOR)
		("inv_src1_color", D3D11_BLEND_INV_SRC1_COLOR)
		("src1_alpha", D3D11_BLEND_SRC1_ALPHA)
		("inv_src1_alpha", D3D11_BLEND_INV_SRC1_ALPHA);

	static auto valid_blend_op = map_list_of
		("add", D3D11_BLEND_OP_ADD)
		("subtract", D3D11_BLEND_OP_SUBTRACT)
		("rev_subtract", D3D11_BLEND_OP_REV_SUBTRACT)
		("min", D3D11_BLEND_OP_MIN)
		("max", D3D11_BLEND_OP_MAX);

	while (!scope->end()) {
		Symbol symbol;
		if (scope->consume_in(valid, &symbol)) {
			EXPECT(scope, kSymEquals);
			string value = scope->next_identifier();
			EXPECT(scope, kSymSemicolon);

			switch (symbol) {
				case kSymBlendEnable:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->BlendEnable);
					break;
				case kSymSrcBlend:
				case kSymDestBlend:
				case kSymSrcBlendAlpha:
				case kSymDestBlendAlpha:
					lookup<string, D3D11_BLEND>(value, valid_blend,
						symbol == kSymSrcBlend ? &desc->SrcBlend : 
						symbol == kSymDestBlend ? &desc->DestBlend :
						symbol == kSymSrcBlendAlpha ? &desc->SrcBlendAlpha :
						&desc->DestBlendAlpha);
					break;
				case kSymBlendOp:
				case kSymBlendOpAlpha:
					lookup<string, D3D11_BLEND_OP>(value, valid_blend_op,
						symbol == kSymBlendOp ? &desc->BlendOp : &desc->BlendOpAlpha);
					break;
				case kSymRenderTargetWriteMask:
					sscanf(value.c_str(), "%ud", &desc->RenderTargetWriteMask);
					break;
			}
		} else {
			return true;
		}
	}
	return false;
}

bool TechniqueParser::parse_blend_desc(Scope *scope, D3D11_BLEND_DESC *desc) {

	static auto valid = list_of(kSymAlphaToCoverageEnable)(kSymIndependentBlendEnable)(kSymRenderTarget);

	while (!scope->end()) {
		Symbol symbol = scope->consume_in(valid);
		if (symbol == kSymAlphaToCoverageEnable || symbol == kSymIndependentBlendEnable) {
			EXPECT(scope, kSymEquals);
			string value = scope->next_identifier();
			EXPECT(scope, kSymSemicolon);
			lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), 
				symbol == kSymAlphaToCoverageEnable ? &desc->AlphaToCoverageEnable : &desc->IndependentBlendEnable);

		} else if (symbol == kSymRenderTarget) {
			EXPECT(scope, kSymListOpen);
			int target = scope->next_int();
			if (target < 0 || target >= 8)
				return false;
			EXPECT(scope, kSymListClose);
			EXPECT(scope, kSymEquals);
			EXPECT(scope, kSymBlockOpen);
			if (!parse_render_target(scope, &desc->RenderTarget[target]))
				return false;
			EXPECT(scope, kSymBlockClose);
			EXPECT(scope, kSymSemicolon);

		} else {
			return true;
		}
	}
	return false;
}

bool TechniqueParser::parse_rasterizer_desc(Scope *scope, D3D11_RASTERIZER_DESC *desc) {

	static auto valid = list_of
		(kSymFillMode)(kSymCullMode)(kSymFrontCounterClockwise)(kSymDepthBias)(kSymDepthBiasClamp)
		(kSymSlopeScaledDepthBias)(kSymDepthClipEnable)
		(kSymScissorEnable)(kSymMultisampleEnable)(kSymAntialiasedLineEnable);

	while (!scope->end()) {
		Symbol symbol;
		if (scope->consume_in(valid, &symbol)) {
			EXPECT(scope, kSymEquals);
			string value = scope->next_identifier();
			EXPECT(scope, kSymSemicolon);

			switch (symbol) {
				case kSymFillMode:
					lookup<string, D3D11_FILL_MODE>(value, map_list_of("wireframe", D3D11_FILL_WIREFRAME)("solid", D3D11_FILL_SOLID), &desc->FillMode);
					break;
				case kSymCullMode:
					lookup<string, D3D11_CULL_MODE>(value, map_list_of("none", D3D11_CULL_NONE)("front", D3D11_CULL_FRONT)("back", D3D11_CULL_BACK), &desc->CullMode);
					break;
				case kSymFrontCounterClockwise:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->FrontCounterClockwise);
					break;
				case kSymDepthBias:
					sscanf(value.c_str(), "%d", &desc->DepthBias);
					break;
				case kSymDepthBiasClamp:
					sscanf(value.c_str(), "%f", &desc->DepthBiasClamp);
					break;
				case kSymSlopeScaledDepthBias:
					sscanf(value.c_str(), "%f", &desc->SlopeScaledDepthBias);
					break;
				case kSymDepthClipEnable:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->DepthClipEnable);
					break;
				case kSymScissorEnable:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->ScissorEnable);
					break;
				case kSymMultisampleEnable:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->MultisampleEnable);
					break;
				case kSymAntialiasedLineEnable:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->AntialiasedLineEnable);
					break;
			}
		} else {
			return true;
		}
	}
	return false;
}

bool TechniqueParser::parse_vertices(Scope *scope, Technique *technique) {

	static auto valid_tags = vector<Symbol>(list_of(kSymFormat)(kSymData));

	while (true) {
		switch (scope->consume_in(valid_tags)) {

		case kSymFormat: {
			string format = scope->string_until(';');
			if (format == "pos") {
				technique->_vertex_size = 3 * sizeof(float);
			} else if (format == "pos_tex") {
				technique->_vertex_size = 5 * sizeof(float);
			} else {
				return false;
			}
			EXPECT(scope, kSymSemicolon);
			break;
		}

		case kSymData:
			EXPECT(scope, kSymListOpen);
			while (scope->consume_if(kSymParenOpen)) {
				int num_verts = technique->_vertex_size / sizeof(float);
				for (int i = 0; i < num_verts; ++i) {
					technique->_vertices.push_back(scope->next_float());
					scope->consume_if(kSymComma);
				}
				EXPECT(scope, kSymParenClose);
				scope->consume_if(kSymComma);
			}
			EXPECT(scope, kSymListClose);
			EXPECT(scope, kSymSemicolon);
			break;

		default:
			if (technique->_vertex_size == -1)
				return false;
			technique->_vb = GRAPHICS.create_static_vertex_buffer(technique->_vertex_size * technique->_vertices.size(), &technique->_vertices[0]);
			return true;
		}
	}
	return true;
}

bool TechniqueParser::parse_indices(Scope *scope, Technique *technique) {

	static auto valid_tags = vector<Symbol>(list_of(kSymFormat)(kSymData));

	technique->_index_format = DXGI_FORMAT_UNKNOWN;
	while (true) {
		switch (scope->consume_in(valid_tags)) {

		case kSymFormat: {
			string format = scope->string_until(';');
			if (format == "index16") {
				technique->_index_format = DXGI_FORMAT_R16_UINT;
			} else if (format == "index32") {
				technique->_index_format = DXGI_FORMAT_R32_UINT;
			} else {
				return false;
			}
			EXPECT(scope, kSymSemicolon);
			break;
		}

		case kSymData: {
			int max_value = INT_MIN;
			EXPECT(scope, kSymListOpen);
			do {
				technique->_indices.push_back(scope->next_int());
				max_value = max(max_value, technique->_indices.back());

			} while (scope->consume_if(kSymComma));

			if (technique->_index_format == DXGI_FORMAT_UNKNOWN)
				technique->_index_format = max_value < (1 << 16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

			EXPECT(scope, kSymListClose);
			EXPECT(scope, kSymSemicolon);
			break;
		}
		default:
			if (technique->_index_format == DXGI_FORMAT_UNKNOWN) {
				return false;
			} else if (technique->_index_format == DXGI_FORMAT_R16_UINT) {
				// create a local copy of the indices
				vector<uint16> v;
				v.reserve(technique->_indices.size());
				for (size_t i = 0; i < technique->_indices.size(); ++i)
					v.push_back((uint16)technique->_indices[i]);
				technique->_ib = GRAPHICS.create_static_index_buffer(index_format_to_size(technique->_index_format) * v.size(), &v[0]);
			} else {
				technique->_ib = GRAPHICS.create_static_index_buffer(index_format_to_size(technique->_index_format) * technique->_indices.size(), &technique->_indices[0]);
			}
			return true;
		}
	}
	return true;
}

bool TechniqueParser::parse_technique(Scope *scope, Technique *technique) {

	static auto valid = list_of
		(kSymVertexShader)(kSymPixelShader)
		(kSymMaterial)
		(kSymVertices)(kSymIndices)
		(kSymRasterizerDesc)(kSymSamplerDesc)(kSymDepthStencilDesc)(kSymBlendDesc);

	while (!scope->end()) {
		Symbol symbol;
		if (scope->consume_in(valid, &symbol)) {
			switch (symbol) {

			case kSymVertexShader:
			case kSymPixelShader: {
				Shader **shader;
				if (symbol == kSymVertexShader) {
					shader = &technique->_vertex_shader;
					*shader = new VertexShader;
				} else {
					shader = &technique->_pixel_shader;
					*shader = new PixelShader;
				}
				EXPECT(scope, kSymBlockOpen);
				if (!parse_shader(scope, *shader)) {
					SAFE_DELETE(*shader);
					return false;
				}
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;
			}

			case kSymMaterial: {
				string technique_name = technique->name();
				// materials are prefixed by the technique name
				Material mat(technique_name + "::" + scope->next_identifier());
				EXPECT(scope, kSymBlockOpen);
				if (!parse_material(scope, &mat))
					return false;
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				MATERIAL_MANAGER.add_material(mat);
				break;
			}

			case kSymRasterizerDesc:
				EXPECT(scope, kSymBlockOpen);
				if (!parse_rasterizer_desc(scope, &technique->_rasterizer_desc))
					return false;
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;

			case kSymSamplerDesc: {
				string name = scope->next_identifier();
				EXPECT(scope, kSymBlockOpen);

				D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
				if (!parse_sampler_desc(scope, &desc))
					return false;
				technique->_sampler_descs.push_back(make_pair(name, desc));
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;
			}

			case kSymDepthStencilDesc:
				EXPECT(scope, kSymBlockOpen);
				if (!parse_depth_stencil_desc(scope, &technique->_depth_stencil_desc))
					return false;
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;

			case kSymBlendDesc:
				EXPECT(scope, kSymBlockOpen);
				if (!parse_blend_desc(scope, &technique->_blend_desc))
					return false;
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;

			case kSymVertices:
				EXPECT(scope, kSymBlockOpen);
				if (!parse_vertices(scope, technique)) {
					// TODO: delete buffer
					return false;
				}
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;

			case kSymIndices:
				EXPECT(scope, kSymBlockOpen);
				if (!parse_indices(scope, technique)) {
					// TODO: delete buffer
					return false;
				}
				EXPECT(scope, kSymBlockClose);
				EXPECT(scope, kSymSemicolon);
				break;
			}
		} else {
			return true;
		}
	}
	return false;
}


bool TechniqueParser::parse_inner(Scope *scope, vector<Technique *> *techniques) {

	while (!scope->end()) {
		switch (scope->next_symbol()) {

		case kSymTechnique: {
			Technique *t = new Technique;
			Rollback rb([&](){delete exch_null(t);});
			t->_name = scope->next_identifier();
			EXPECT(scope, kSymBlockOpen);
			parse_technique(scope, t);
			EXPECT(scope, kSymBlockClose);
			EXPECT(scope, kSymSemicolon);
			rb.commit();
			techniques->push_back(t);
			break;
		}

		default:
			return false;
		}
	}

	return true;
}

bool TechniqueParser::parse(const char *start, const char *end, vector<Technique *> *techniques) {
	
	Scope scope(_symbol_trie, start, end-1);
	return parse_inner(&scope, techniques);
}
