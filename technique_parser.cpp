#include "stdafx.h"
#include "technique.hpp"
#include "technique_parser.hpp"
#include "property_manager.hpp"

using namespace std;
using namespace boost::assign;

namespace technique_parser_details {

enum Symbol {
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

	kSymTechnique,
	kSymId,
	kSymVertexShader,
	kSymPixelShader,
		kSymFile,
		kSymEntryPoint,
		kSymParams,

	kSymFloat,
	kSymFloat2,
	kSymFloat3,
	kSymFloat4,
	kSymFloat4x4,
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

	{ kSymTechnique, "technique" },

	{ kSymVertexShader, "vertex_shader" },
	{ kSymPixelShader, "pixel_shader" },
		{ kSymFile, "file" },
		{ kSymEntryPoint, "entry_point" },
		{ kSymParams, "params" },

	// params
	{ kSymFloat, "float" },
	{ kSymFloat, "float1" },
	{ kSymFloat2, "float2" },
	{ kSymFloat3, "float3" },
	{ kSymFloat4, "float4" },
	{ kSymFloat4x4, "float4x4" },
	{ kSymInt, "int" },
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

using namespace technique_parser_details;

TechniqueParser::TechniqueParser() : _symbol_trie(new Trie()) {
	for (int i = 0; i < ELEMS_IN_ARRAY(g_symbols); ++i)
		_symbol_trie->add_symbol(g_symbols[i].str, strlen(g_symbols[i].str), g_symbols[i].symbol);
}

TechniqueParser::~TechniqueParser() {
	SAFE_DELETE(_symbol_trie);
}

const char *TechniqueParser::next_symbol(const char *start, const char *end, Symbol *symbol) {
	if (skip_whitespace(&start, end) == end)
		return end;

	int len = 0;
	Symbol s = _symbol_trie->find_symbol(start, end - start, &len);
	// skip comments
	if (s == kSymSingleLineComment) {
		// skip until CR-LF
		start += len;
		while (start < end && *start != '\r')
			++start;
		if (start < end && start[1] == '\n')
			++start;
		return next_symbol(start, end, symbol);

	} else if (s == kSymMultiLineCommentStart) {
		start += len;
		while (start < end - 1 && start[0] != '*' && start[1] != '/')
			++start;
		if (start == end - 1)
			return end;
		return next_symbol(start+2, end, symbol);

	} else {
		*symbol = s;
	}

	return skip_whitespace(start + len, end);
}

const char *TechniqueParser::next_int(const char *start, const char *end, int *out) {
	*out = 0;
	start = skip_whitespace(&start, end);
	if (start == end)
		return end;
	bool neg = false;
	if (*start == '-') {
		neg = true;
		++start;
	}

	int res = 0;
	while (start < end && isdigit((uint8_t)*start)) {
		res = res * 10 + (*start - '0');
		++start;
	}
	if (neg)
		res = -res;
	*out = res;
	return skip_whitespace(start, end);
}

const char *TechniqueParser::next_identifier(const char *start, const char *end, string *id) {
	const char *tmp = skip_whitespace(&start, end);
	while (tmp < end && (isalnum((uint8_t)*tmp) || *tmp == '_'))
		++tmp;

	if (tmp == start)
		return end;
	*id = string(start, tmp - start);
	return skip_whitespace(tmp, end);
}

bool TechniqueParser::expect(Symbol symbol, const char **start, const char *end) {
	Symbol tmp;
	*start = next_symbol(*start, end, &tmp);
	return tmp == symbol;
}

Symbol TechniqueParser::peek(const char *start, const char *end) {
	Symbol tmp;
	next_symbol(start, end, &tmp);
	return tmp;
}

#define CHOMP(x, s, e) if (!expect((x), &(s), (e))) return e;

const char *TechniqueParser::string_until(const char *start, const char *end, char delim, string *res) {
	start = skip_whitespace(start, end);
	const char *tmp = start;
	while (start < end && *start != delim)
		++start;

	if (start == end) {
		return end;
	}

	*res = string(tmp, start - tmp);
	return start;
}

void TechniqueParser::parse_value(const string &value, ShaderParam *param) {
	switch (param->type) {
	case Property::kFloat:
		sscanf(value.c_str(), "%f", &param->default_value._float[0]);
		break;
	case Property::kFloat2:
		sscanf(value.c_str(), "%f %f", &param->default_value._float[0], &param->default_value._float[1]);
		break;
	case Property::kFloat3:
		sscanf(value.c_str(), "%f %f %f", &param->default_value._float[0], &param->default_value._float[1], &param->default_value._float[2]);
		break;
	case Property::kFloat4:
		sscanf(value.c_str(), "%f %f %f %f", &param->default_value._float[0], &param->default_value._float[1], &param->default_value._float[2], &param->default_value._float[3]);
		break;
	}
}

const char *TechniqueParser::parse_param(const char *start, const char *end, ShaderParam *param) {
	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockOpen, start, end);

	// type, name and source are required. the default value is optional
	Symbol type;
	start = next_symbol(start, block_end, &type);
	param->type = lookup_default<Symbol, Property::Enum>(type, map_list_of
		(kSymFloat, Property::kFloat)
		(kSymFloat2, Property::kFloat2)
		(kSymFloat3, Property::kFloat3)
		(kSymFloat4, Property::kFloat4)
		(kSymFloat4x4, Property::kFloat4x4),
		Property::kUnknown);

	if (param->type == Property::kUnknown)
		return end;

	start = next_identifier(start, end, &param->name);

	string source;
	start = next_identifier(start, end, &source);
	param->source = lookup_default<string, ShaderParam::Source::Enum>(source, map_list_of
		("material", ShaderParam::Source::kMaterial)
		("system", ShaderParam::Source::kSystem)
		("user", ShaderParam::Source::kUser)
		("mesh", ShaderParam::Source::kMesh),
		ShaderParam::Source::kUnknown);
	if (param->source == ShaderParam::Source::kUnknown)
		return end;

	CHOMP(kSymSemicolon, start, end);

	// check for default value
	if (kSymParamDefault == peek(start, block_end)) {
		CHOMP(kSymParamDefault, start, end);
		string value;
		start = string_until(start, end, ';', &value);
		CHOMP(kSymSemicolon, start, end);
		parse_value(value, param);
	}

	CHOMP(kSymBlockClose, block_end, end);
	return block_end;
}

const char *TechniqueParser::parse_params(const char *start, const char *end, vector<ShaderParam> *params) {
	const char *block_end = scope_extents(start, end, '[', ']');
	CHOMP(kSymListOpen, start, block_end);
	while (start != block_end) {
		params->push_back(ShaderParam());
		start = parse_param(start, block_end, &params->back());
		if (kSymComma == peek(start, block_end)) {
			CHOMP(kSymComma, start, block_end);
		} else {
			break;
		}
	}
	CHOMP(kSymListClose, start, end);
	return expect(kSymSemicolon, &start, end) ? start: end;
}

const char *TechniqueParser::parse_shader(const char *start, const char *end, Shader *shader) {
	const char *block_end = scope_extents(start, end, '{', '}');

	CHOMP(kSymBlockOpen, start, block_end);

	while (start < block_end) {
		Symbol symbol;
		start = next_symbol(start, block_end, &symbol);
		if (symbol == kSymFile) {
			start = string_until(start, block_end, ';', &shader->filename);
			CHOMP(kSymSemicolon, start, block_end);
		} else if (symbol == kSymEntryPoint) {
			start = string_until(start, block_end, ';', &shader->entry_point);
			CHOMP(kSymSemicolon, start, block_end);
		} else if (symbol == kSymParams) {
			start = parse_params(start, block_end, &shader->params);
		} else {
			start = block_end;
		}
	}

	CHOMP(kSymBlockClose, block_end, end);
	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}

const char *TechniqueParser::parse_rasterizer_desc(const char *start, const char *end, D3D11_RASTERIZER_DESC *desc) {

	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockOpen, start, end);
	while (start < block_end) {
		Symbol symbol;
		start = next_symbol(start, end, &symbol);
		CHOMP(kSymEquals, start, end);
		string value;
		start = next_identifier(start, end, &value);
		CHOMP(kSymSemicolon, start, end);

		switch (symbol) {
		case kSymFillMode:
			lookup<string, D3D11_FILL_MODE>(value, map_list_of("wireframe", D3D11_FILL_WIREFRAME)("solid", D3D11_FILL_SOLID), &desc->FillMode);
			break;
		case kSymCullMode:
			lookup<string, D3D11_CULL_MODE>(value, map_list_of("wireframe", D3D11_CULL_NONE)("front", D3D11_CULL_FRONT)("back", D3D11_CULL_BACK), &desc->CullMode);
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
	}
	CHOMP(kSymBlockClose, block_end, end);
	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}

const char *TechniqueParser::parse_sampler_desc(const char *start, const char *end, D3D11_SAMPLER_DESC *desc) {
	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockOpen, start, end);
	while (start < block_end) {
		Symbol symbol;
		start = next_symbol(start, end, &symbol);
		CHOMP(kSymEquals, start, end);
		string value;
		start = string_until(start, end, ';', &value);
		CHOMP(kSymSemicolon, start, end);

		switch (symbol) {
		case kSymFilter:
			lookup<string, D3D11_FILTER>(value, map_list_of
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
				("comparison_anisotropic", D3D11_FILTER_COMPARISON_ANISOTROPIC),
				&desc->Filter);
			break;
		case kSymAddressU:
		case kSymAddressV:
		case kSymAddressW:
			lookup<string, D3D11_TEXTURE_ADDRESS_MODE>(value, map_list_of
				("wrap", D3D11_TEXTURE_ADDRESS_WRAP)
				("mirror", D3D11_TEXTURE_ADDRESS_MIRROR)
				("clamp", D3D11_TEXTURE_ADDRESS_CLAMP)
				("border", D3D11_TEXTURE_ADDRESS_BORDER)
				("mirror_once", D3D11_TEXTURE_ADDRESS_MIRROR_ONCE), 
				symbol == kSymAddressU ? &desc->AddressU : symbol == kSymAddressV ? &desc->AddressV : &desc->AddressW);
			break;
		case kSymMipLODBias:
			sscanf(value.c_str(), "%f", &desc->MipLODBias);
			break;
		case kSymMaxAnisotropy:
			sscanf(value.c_str(), "%ud", &desc->MaxAnisotropy);
			break;
		case kSymComparisonFunc:
			lookup<string, D3D11_COMPARISON_FUNC>(value, map_list_of
				("never", D3D11_COMPARISON_NEVER)
				("less", D3D11_COMPARISON_LESS)
				("equal", D3D11_COMPARISON_EQUAL)
				("less_equal", D3D11_COMPARISON_LESS_EQUAL)
				("greater", D3D11_COMPARISON_GREATER)
				("not_equal", D3D11_COMPARISON_NOT_EQUAL)
				("greater_equal", D3D11_COMPARISON_GREATER_EQUAL)
				("always", D3D11_COMPARISON_ALWAYS), 
				&desc->ComparisonFunc);
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
	}
	CHOMP(kSymBlockClose, block_end, end);
	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}

const char *TechniqueParser::parse_render_target(const char *start, const char *end, D3D11_RENDER_TARGET_BLEND_DESC *desc) {
	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockOpen, start, end);
	while (start < block_end) {
		Symbol symbol;
		start = next_symbol(start, end, &symbol);
		CHOMP(kSymEquals, start, end);
		string value;
		start = string_until(start, end, ';', &value);
		CHOMP(kSymSemicolon, start, end);

		switch (symbol) {
		case kSymBlendEnable:
			lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->BlendEnable);
			break;
		case kSymSrcBlend:
		case kSymDestBlend:
		case kSymSrcBlendAlpha:
		case kSymDestBlendAlpha:
			lookup<string, D3D11_BLEND>(value, map_list_of
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
				("inv_src1_alpha", D3D11_BLEND_INV_SRC1_ALPHA),
				symbol == kSymSrcBlend ? &desc->SrcBlend : 
				symbol == kSymDestBlend ? &desc->DestBlend :
				symbol == kSymSrcBlendAlpha ? &desc->SrcBlendAlpha :
				&desc->DestBlendAlpha);
			break;
		case kSymBlendOp:
		case kSymBlendOpAlpha:
			lookup<string, D3D11_BLEND_OP>(value, map_list_of
				("add", D3D11_BLEND_OP_ADD)
				("subtract", D3D11_BLEND_OP_SUBTRACT)
				("rev_subtract", D3D11_BLEND_OP_REV_SUBTRACT)
				("min", D3D11_BLEND_OP_MIN)
				("max", D3D11_BLEND_OP_MAX),
				symbol == kSymBlendOp ? &desc->BlendOp : &desc->BlendOpAlpha);
			break;
		case kSymRenderTargetWriteMask:
			sscanf(value.c_str(), "%ud", &desc->RenderTargetWriteMask);
			break;
		}
	}

	CHOMP(kSymBlockClose, block_end, end);
	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}

const char *TechniqueParser::parse_blend_desc(const char *start, const char *end, D3D11_BLEND_DESC *desc) {
	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockOpen, start, end);
	while (start < block_end) {
		Symbol symbol;
		start = next_symbol(start, end, &symbol);
		string value;
		switch (symbol) {
		case kSymAlphaToCoverageEnable:
			CHOMP(kSymEquals, start, end);
			start = string_until(start, end, ';', &value);
			CHOMP(kSymSemicolon, start, end);
			lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->AlphaToCoverageEnable);
			break;
		case kSymIndependentBlendEnable:
			CHOMP(kSymEquals, start, end);
			start = string_until(start, end, ';', &value);
			CHOMP(kSymSemicolon, start, end);
			lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->IndependentBlendEnable);
			break;
		case kSymRenderTarget:
			{
				CHOMP(kSymListOpen, start, end);
				int target;
				start = next_int(start, end, &target);
				if (target >= 8)
					return end;
				CHOMP(kSymListClose, start, end);
				CHOMP(kSymEquals, start, end);
				start = parse_render_target(start, end, &desc->RenderTarget[target]);
			}
			break;
		}
	}
	CHOMP(kSymBlockClose, block_end, end);
	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}

const char *TechniqueParser::parse_depth_stencil_desc(const char *start, const char *end, D3D11_DEPTH_STENCIL_DESC *desc) {
	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockClose, block_end, end);
	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}


const char *TechniqueParser::parse_technique(const char *start, const char *end, Technique *technique) {
	start = next_identifier(start, end, &technique->_name);
	const char *block_end = scope_extents(start, end, '{', '}');
	CHOMP(kSymBlockOpen, start, end);

	Symbol symbol;
	while (start < block_end) {
		start = next_symbol(start, block_end, &symbol);
		switch (symbol) {
		case kSymBlockClose:
			if (start == block_end)
				break;
			return end;
			break;
		case kSymVertexShader:
			technique->_vertex_shader = new VertexShader;
			start = parse_shader(start, block_end, technique->_vertex_shader);
			break;
		case kSymPixelShader:
			technique->_pixel_shader = new PixelShader;
			start = parse_shader(start, block_end, technique->_pixel_shader);
			break;
		case kSymRasterizerDesc:
			start = parse_rasterizer_desc(start, block_end, &technique->_rasterizer_desc);
			break;
		case kSymSamplerDesc:
			start = parse_sampler_desc(start, block_end, &technique->_sampler_desc);
			break;
		case kSymDepthStencilDesc:
			start = parse_depth_stencil_desc(start, block_end, &technique->_depth_stencil_desc);
			break;
		case kSymBlendDesc:
			start = parse_blend_desc(start, block_end, &technique->_blend_desc);
			break;
		default:
			return end;
			break;
		}
	}

	return expect(kSymSemicolon, &block_end, end) ? block_end : end;
}

const char *TechniqueParser::skip_whitespace(const char **start, const char *end) {
	while (*start < end && isspace((uint8_t)**start))
		(*start)++;
	return *start;
}

const char *TechniqueParser::skip_whitespace(const char *start, const char *end) {
	return skip_whitespace(&start, end);
}

const char *TechniqueParser::scope_extents(const char *start, const char *end, char scope_open, char scope_close) {
	// NOTE: scope_extents returns a pointer to the last character in the scope (the closing symbol)
	// and not one past, like most other functions
	if (skip_whitespace(&start, end) == end || *start != scope_open)
		return NULL;

	int balance = 0;
	while (start < end) {
		char ch = *start;
		if (ch == scope_open) {
			++balance;
		} else if (ch == scope_close) {
			if (--balance == 0)
				return start;
		}
		start++;
	}
	return end;
}

bool TechniqueParser::parse_main(const char *start, const char *end, Technique *technique) {

	Symbol symbol;
	start = next_symbol(start, end, &symbol);
	if (symbol == kSymTechnique) {
		start = parse_technique(start, end, technique);
		return true;
	}

	return false;
}
