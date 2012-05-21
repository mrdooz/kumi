#include "stdafx.h"
#include "technique_parser.hpp"
#include "technique.hpp"
#include "string_utils.hpp"
#include "file_utils.hpp"
#include "property.hpp"
#include "material.hpp"
#include "graphics_interface.hpp"
#include "logger.hpp"
#include "tracked_location.hpp"

using namespace std;
using namespace boost::assign;

struct parser_exception : public exception {
	parser_exception(const string& str) : str(str) {}
	virtual const char *what() const { return str.c_str(); }
	string str;
};

#define THROW_ON_FALSE(x) if (!(x)) throw parser_exception(__FUNCTION__ ":" #x);

static DXGI_FORMAT index_size_to_format(int size) {
	if (size == 2) return DXGI_FORMAT_R16_UINT;
	if (size == 4) return DXGI_FORMAT_R32_UINT;
	return DXGI_FORMAT_UNKNOWN;
}

static int index_format_to_size(DXGI_FORMAT fmt) {
	if (fmt == DXGI_FORMAT_R16_UINT) return 2;
	if (fmt == DXGI_FORMAT_R32_UINT) return 4;
	return 0;
}

static void parse_int(const char *str, int *v) {
	THROW_ON_FALSE(sscanf(str, "%d", v) == 1);
}

static void parse_float(const char *str, float *v) {
	THROW_ON_FALSE(sscanf(str, "%f", v) == 1);
}

static float parse_float(const string &str) {
	float v;
	THROW_ON_FALSE(sscanf(str.c_str(), "%f", &v) == 1);
	return v;
}

static int parse_int(const string &str) {
	int a;
	parse_int(str.c_str(), &a);
	return a;
}

static void parse_float2(const char *str, float *v1, float *v2) {
	THROW_ON_FALSE(sscanf(str, "%f %f", v1, v2) == 2 || sscanf(str, "%f, %f", v1, v2) == 2);
}

static void parse_float3(const char *str, float *v1, float *v2, float *v3) {
	THROW_ON_FALSE(sscanf(str, "%f %f %f", v1, v2, v3) == 3 || sscanf(str, "%f, %f, %f", v1, v2, v3) == 3);
}

static void parse_float4(const char *str, float *v1, float *v2, float *v3, float *v4) {
	THROW_ON_FALSE(sscanf(str, "%f %f %f %f", v1, v2, v3, v4) == 4 || sscanf(str, "%f, %f, %f, %f", v1, v2, v3, v4) == 4);
}

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
  kSymColor,
	kSymFloat4x4,
	kSymTexture2d,
	kSymSampler,
	kSymInt,
	kSymDefault,

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
  { kSymColor, "color" },
	{ kSymFloat4, "float4" },
	{ kSymFloat4x4, "float4x4" },
	{ kSymInt, "int" },
	{ kSymTexture2d, "texture2d" },
	{ kSymSampler, "sampler" },
	{ kSymDefault, "default" },

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

	Symbol find_symbol(const string &str) {
		return find_symbol(str.c_str(), str.size(), nullptr);
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
				if (match_length)
					*match_length = last_len;
				return last_match->symbol;
			}
			return kSymUnknown;
		}

		if (match_length)
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

static void parse_value(const string &value, PropertyType::Enum type, float *out) {
	const char *v = value.c_str();
	switch (type) {
		case PropertyType::kFloat:
			parse_float(v, &out[0]);
			break;
		case PropertyType::kFloat2:
			parse_float2(v, &out[0], &out[1]);
			break;
		case PropertyType::kFloat3:
			parse_float3(v, &out[0], &out[1], &out[2]);
			break;
		case PropertyType::kFloat4:
			parse_float4(v, &out[0], &out[1], &out[2], &out[3]);
			break;
		default:
			// throw?
			break;
	}
}

using namespace technique_parser_details;

TechniqueParser::TechniqueParser() 
	: _symbol_trie(new Trie()) 
{
	for (int i = 0; i < ELEMS_IN_ARRAY(g_symbols); ++i) {
		_symbol_trie->add_symbol(g_symbols[i].str, strlen(g_symbols[i].str), g_symbols[i].symbol);
		_symbol_to_string[g_symbols[i].symbol] = g_symbols[i].str;
	}
}

TechniqueParser::~TechniqueParser() {
}

struct Scope {
	// start and end point to the first and last character of the scope (inclusive)
	Scope(TechniqueParser *parser, const char *start, const char *end) : _parser(parser), _org_start(start), _start(start), _end(end) {}

	bool is_valid() const { return _org_start != nullptr; }
	bool end() const { return _start >= _end; }
	Scope &advance(const Scope &inner) {
		// advance the scope to point to the character after the inner scope
		if (inner._start < _start || inner._end >= _end)
			THROW_ON_FALSE(false);
		_start = inner._end + 1;
		return *this;
	}

	Scope create_delimited_scope(char open, char close) {
		int balance = 0;
		const char *p = _start;
		const char *start_inner = nullptr, *end_inner = nullptr;
		while (true) {
			if (p >= _end)
				break;
			char ch = *p;
			if (ch == open) {
				if (balance++ == 0)
					start_inner = p + 1;
			} else if (ch == close) {
				if (--balance == 0) {
					end_inner = p;
					return Scope(_parser, start_inner, end_inner).skip_whitespace();
				}
			}
			++p;
		}
		throw parser_exception(__FUNCTION__);
	}

	Scope &munch(Symbol symbol) {
		if (next_symbol() != symbol) {
			log_error(_parser->_symbol_to_string[symbol].c_str());
			throw parser_exception(__FUNCTION__);
		}
		return *this;
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
		Symbol s = _parser->_symbol_trie->find_symbol(_start, _end - _start + 1, &len);

		_start += len;
		skip_whitespace();
		return s;
	}

	Scope &skip_whitespace() {
		bool removed;
		do {
			removed = false;
			while (_start <= _end && isspace((uint8_t)*_start))
				++_start;

			if (_start == _end)
				return *this;

			int len = 0;
			Symbol s = _parser->_symbol_trie->find_symbol(_start, _end - _start + 1, &len);

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
					return *this;
				_start += 2;
			}

		} while(removed);
		return *this;
	}

	int next_int() {
		bool sign = *_start == '-' || *_start == '+';
		int res = atoi(_start);
		_start += (res != 0 ? (int)log10(abs((double)res)) : 0) + 1 + (sign ? 1 : 0);
		skip_whitespace();
		return res;
	}

	float next_float() {
		double r = (double)next_int();
		double frac = 0;
		if (*_start == '.') {
			++_start;
			double mul = 0.1;
			while (_start <= _end && isdigit((byte)*_start)) {
				frac += *_start - '0' * mul;
				mul /= 10;
				_start++;
			}
		}
		skip_whitespace();
		return (float)(r + frac);
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

	Scope &next_identifier(string *res) {
		*res = next_identifier();
		return *this;
	}

	char next_char() {
		return *_start++;
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

	TechniqueParser *_parser;
	const char *_org_start;
	const char *_start;
	const char *_end;
};

template<typename Key, typename Value>
Value lookup_throw(Key str, const std::unordered_map<Key, Value> &candidates) {
	auto it = candidates.find(str);
	THROW_ON_FALSE(it != candidates.end());
	return it->second;
}

void TechniqueParser::parse_param(const vector<string> &param, Shader *shader) {

  auto valid_sources = map_list_of
    ("material", PropertySource::kMaterial)
    ("system", PropertySource::kSystem)
    ("user", PropertySource::kUser)
    ("mesh", PropertySource::kMesh);
  //("technique", PropertySource::kTechnique);

  THROW_ON_FALSE(param.size() >= 3);

  // type, name and source are required. the default value is optional
  Symbol sym_type = _symbol_trie->find_symbol(param[0]);
  PropertyType::Enum type = lookup_throw<Symbol, PropertyType::Enum>(sym_type, valid_property_types);

  string name = param[1];
  string friendly_name = param.size() == 4 ? param[2] : "";
  int src_pos = friendly_name.empty() ? 2 : 3;
  PropertySource::Enum source = lookup_throw<string, PropertySource::Enum>(param[src_pos], valid_sources);

  bool cbuffer_param = false;
/*
  if (type == PropertyType::kSampler) {
    shader->sampler_params().push_back(SamplerParam(name, type, source));
  } else 
*/
  if (type == PropertyType::kTexture2d) {
    shader->resource_view_params().push_back(ResourceViewParam(name, type, source, friendly_name));
  } else {
    shader->cbuffer_params().push_back(CBufferParam(name, type, source));
    cbuffer_param = true;
  }

    // TODO: add defaults again
    /*
    EXPECT(scope, kSymSemicolon);

    // check for default value
    if (cbuffer_param && scope->consume_if(kSymDefault)) {
    string value = scope->string_until(';');
    EXPECT(scope, kSymSemicolon);
    parse_value(value, shader->cbuffer_params.back().type, &shader->cbuffer_params.back().default_value._float[0]);
    }
    */
}

template<typename T> T id(T t) { return t; }

template <typename T>
void parse_list(Scope *scope, vector<vector<T> > *items, function<T(const string &)> xform = id<string>) {
	// a list is a list of tuples, where items within the tuple are whitespace delimited, and
	// tuples are delimited by ';'
	const char *item_start = scope->_start;
	vector<T> cur;
	while (!scope->end()) {
		char ch = scope->next_char();
		if (ch == ';') {
			cur.push_back(xform(string(item_start, scope->_start - item_start - 1)));
			items->push_back(cur);
			cur.clear();
			item_start = scope->skip_whitespace()._start;
		} else if (isspace((uint8_t)ch)) {
			cur.push_back(xform(string(item_start, scope->_start - item_start - 1)));
			item_start = scope->skip_whitespace()._start;
		}
	}

	if (item_start < scope->_end) {
		cur.push_back(xform(string(item_start, scope->_start - item_start)));
		items->push_back(cur);
	}
}

void TechniqueParser::parse_material(Scope *scope, Material *material) {

  auto tmp = list_of(kSymFloat)(kSymFloat2)(kSymFloat3)(kSymFloat4);

  Symbol type;
  while (scope->consume_in(tmp, &type)) {
    PropertyType::Enum prop_type = lookup_throw<Symbol, PropertyType::Enum>(type, valid_property_types);
    string name = scope->next_identifier();
    scope->munch(kSymEquals);
    string value = scope->string_until(';');
    float out[16];
    parse_value(value, prop_type, out);
    scope->munch(kSymSemicolon);
    switch (prop_type) {
      case PropertyType::kFloat:
        material->add_property(name, prop_type, out[4]);
        break;
      case PropertyType::kColor:
      case PropertyType::kFloat4:
        material->add_property(name, prop_type, XMFLOAT4(out[0], out[1], out[2], out[3]));
        break;
      default:
        THROW_ON_FALSE(false);
    }
  }
}

void TechniqueParser::parse_shader(Scope *scope, Shader *shader) {

	auto tmp = list_of(kSymFile)(kSymEntryPoint)(kSymParams);
	const string &ext = shader->type() == Shader::kVertexShader ? ".vso" : ".pso";
	string entry_point = shader->type() == Shader::kVertexShader ? "vs_main" : "ps_main";
	Symbol symbol;

	while (scope->consume_in(tmp, &symbol)) {

		switch (symbol) {

			case kSymFile: {
				shader->set_source_filename(scope->string_until(';'));
				// Use the default entry point name until we get a real tag
				shader->set_obj_filename(strip_extension(shader->source_filename().c_str()) + "_" + entry_point + ext);
				scope->munch(kSymSemicolon);
				break;
			}

			case kSymEntryPoint:
				 shader->set_entry_point(entry_point = scope->string_until(';'));
				// update the obj filename
				if (!shader->source_filename().empty())
					shader->set_obj_filename(strip_extension(shader->source_filename().c_str()) + "_" + entry_point + ext);
				scope->munch(kSymSemicolon);
				break;

			case kSymParams : {
				Scope inner = scope->create_delimited_scope('[', ']');
				vector<vector<string>> params;
				parse_list(&inner, &params);
				for (auto it = begin(params); it != end(params); ++it)
					parse_param(*it, shader);
				scope->advance(inner).munch(kSymSemicolon);
				break;
			}
		}
	}
}

void TechniqueParser::parse_depth_stencil_desc(Scope *scope, D3D11_DEPTH_STENCIL_DESC *desc) {

	auto tmp = list_of(kSymDepthEnable);
	Symbol symbol;

	while (scope->consume_in(tmp, &symbol)) {
		string value;
		scope->munch(kSymEquals).next_identifier(&value).munch(kSymSemicolon);
		switch (symbol) {
			case kSymDepthEnable:
				desc->DepthEnable = lookup_throw<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE));
				break;
		}
	}
}

void TechniqueParser::parse_sampler_desc(Scope *scope, D3D11_SAMPLER_DESC *desc) {

	auto valid = list_of
		(kSymFilter)(kSymAddressU)(kSymAddressV)(kSymAddressW)
		(kSymMipLODBias)(kSymMaxAnisotropy)(kSymComparisonFunc)(kSymBorderColor)(kSymMinLOD)(kSymMaxLOD);

	auto valid_filters = map_list_of
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

	auto valid_address = map_list_of
		("wrap", D3D11_TEXTURE_ADDRESS_WRAP)
		("mirror", D3D11_TEXTURE_ADDRESS_MIRROR)
		("clamp", D3D11_TEXTURE_ADDRESS_CLAMP)
		("border", D3D11_TEXTURE_ADDRESS_BORDER)
		("mirror_once", D3D11_TEXTURE_ADDRESS_MIRROR_ONCE);

	auto valid_cmp_func = map_list_of
		("never", D3D11_COMPARISON_NEVER)
		("less", D3D11_COMPARISON_LESS)
		("equal", D3D11_COMPARISON_EQUAL)
		("less_equal", D3D11_COMPARISON_LESS_EQUAL)
		("greater", D3D11_COMPARISON_GREATER)
		("not_equal", D3D11_COMPARISON_NOT_EQUAL)
		("greater_equal", D3D11_COMPARISON_GREATER_EQUAL)
		("always", D3D11_COMPARISON_ALWAYS);

	Symbol symbol;
	while (!scope->end() && scope->consume_in(valid, &symbol)) {
		string value;
		scope->munch(kSymEquals).next_identifier(&value).munch(kSymSemicolon);
		const char *v = value.c_str();

		switch (symbol) {
		case kSymFilter:
			lookup<string, D3D11_FILTER>(value, valid_filters, &desc->Filter);
			break;

		case kSymAddressU:
		case kSymAddressV:
		case kSymAddressW:
			(symbol == kSymAddressU ? desc->AddressU : symbol == kSymAddressV ? desc->AddressV : desc->AddressW) = 
				lookup_throw<string, D3D11_TEXTURE_ADDRESS_MODE>(value, valid_address);
			break;

		case kSymMipLODBias:
			parse_float(v, &desc->MipLODBias);
			break;

		case kSymMaxAnisotropy:
			sscanf(value.c_str(), "%ud", &desc->MaxAnisotropy);
			break;

		case kSymComparisonFunc:
			desc->ComparisonFunc = lookup_throw<string, D3D11_COMPARISON_FUNC>(value, valid_cmp_func);
			break;

		case kSymBorderColor:
			parse_float4(v, &desc->BorderColor[0], &desc->BorderColor[1], &desc->BorderColor[2], &desc->BorderColor[3]);
			break;

		case kSymMinLOD:
			parse_float(v, &desc->MinLOD);
			break;

		case kSymMaxLOD:
			parse_float(v, &desc->MaxLOD);
			break;
		}
	}
}

void TechniqueParser::parse_render_target(Scope *scope, D3D11_RENDER_TARGET_BLEND_DESC *desc) {

	auto valid = list_of
		(kSymBlendEnable)(kSymSrcBlend)(kSymDestBlend)(kSymSrcBlendAlpha)
		(kSymDestBlendAlpha)(kSymBlendOp)(kSymBlendOpAlpha)(kSymRenderTargetWriteMask);

	auto valid_blend = map_list_of
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

	auto valid_blend_op = map_list_of
		("add", D3D11_BLEND_OP_ADD)
		("subtract", D3D11_BLEND_OP_SUBTRACT)
		("rev_subtract", D3D11_BLEND_OP_REV_SUBTRACT)
		("min", D3D11_BLEND_OP_MIN)
		("max", D3D11_BLEND_OP_MAX);

	Symbol symbol;
	while (!scope->end() && scope->consume_in(valid, &symbol)) {
		string value;
		scope->munch(kSymEquals).next_identifier(&value).munch(kSymSemicolon);

		switch (symbol) {
		case kSymBlendEnable:
			desc->BlendEnable = lookup_throw<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE));
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
	}
}

void TechniqueParser::parse_blend_desc(Scope *scope, D3D11_BLEND_DESC *desc) {

	auto valid = list_of(kSymAlphaToCoverageEnable)(kSymIndependentBlendEnable)(kSymRenderTarget);

	Symbol symbol;
	while (!scope->end() && scope->consume_in(valid, &symbol)) {
		if (symbol == kSymAlphaToCoverageEnable || symbol == kSymIndependentBlendEnable) {
			string value;
			scope->munch(kSymEquals).next_identifier(&value).munch(kSymSemicolon);
			lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), 
				symbol == kSymAlphaToCoverageEnable ? &desc->AlphaToCoverageEnable : &desc->IndependentBlendEnable);

		} else if (symbol == kSymRenderTarget) {
			scope->munch(kSymListOpen);
			int target = scope->next_int();
			THROW_ON_FALSE(target >= 0 && target < 7);
			scope->munch(kSymListClose).munch(kSymEquals).munch(kSymBlockOpen);
			parse_render_target(scope, &desc->RenderTarget[target]);
			scope->munch(kSymBlockClose).munch(kSymSemicolon);
		}
	}
}

void TechniqueParser::parse_rasterizer_desc(Scope *scope, D3D11_RASTERIZER_DESC *desc) {

	auto valid = list_of
		(kSymFillMode)(kSymCullMode)(kSymFrontCounterClockwise)(kSymDepthBias)(kSymDepthBiasClamp)
		(kSymSlopeScaledDepthBias)(kSymDepthClipEnable)
		(kSymScissorEnable)(kSymMultisampleEnable)(kSymAntialiasedLineEnable);

	while (!scope->end()) {
		Symbol symbol;
		if (scope->consume_in(valid, &symbol)) {
			string value = scope->munch(kSymEquals).next_identifier();
			scope->munch(kSymSemicolon);

			switch (symbol) {
				case kSymFillMode:
					desc->FillMode = lookup_throw<string, D3D11_FILL_MODE>(value, map_list_of("wireframe", D3D11_FILL_WIREFRAME)("solid", D3D11_FILL_SOLID));
					break;
				case kSymCullMode:
					lookup<string, D3D11_CULL_MODE>(value, map_list_of("none", D3D11_CULL_NONE)("front", D3D11_CULL_FRONT)("back", D3D11_CULL_BACK), &desc->CullMode);
					break;
				case kSymFrontCounterClockwise:
					lookup<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE), &desc->FrontCounterClockwise);
					break;
				case kSymDepthBias:
					parse_int(value.c_str(), &desc->DepthBias);
					break;
				case kSymDepthBiasClamp:
					parse_float(value.c_str(), &desc->DepthBiasClamp);
					break;
				case kSymSlopeScaledDepthBias:
					parse_float(value.c_str(), &desc->SlopeScaledDepthBias);
					break;
				case kSymDepthClipEnable:
					desc->DepthClipEnable = lookup_throw<string, BOOL>(value, map_list_of("true", TRUE)("false", FALSE));
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
			return;
		}
	}
	THROW_ON_FALSE(false);
}

void TechniqueParser::parse_vertices(GraphicsInterface *graphics, Scope *scope, Technique *technique) {

	auto valid_tags = vector<Symbol>(list_of(kSymFormat)(kSymData));
	auto vertex_fmts = map_list_of("pos", 3 * sizeof(float))("pos_tex", 5 * sizeof(float));

	Symbol symbol;
	while (scope->consume_in(valid_tags, &symbol)) {
		switch (symbol) {
			case kSymFormat: {
				technique->_vertex_size = lookup_throw<string, int>(scope->string_until(';'), vertex_fmts);
				scope->munch(kSymSemicolon);
				break;
			}

			case kSymData: {
				Scope inner = scope->create_delimited_scope('[', ']');
				scope->advance(inner).munch(kSymSemicolon);
				vector<vector<float>> verts;
				parse_list<float>(&inner, &verts, [&](const string &x){ return parse_float(x); });
				for (auto it = begin(verts); it != end(verts); ++it) {
					for (auto jt = begin(*it); jt != end(*it); ++jt) {
						technique->_vertices.push_back(*jt);
					}
				}
				break;
			}
		}
	}
	THROW_ON_FALSE(technique->_vertex_size != -1);
	technique->_vb = graphics->create_static_vertex_buffer(FROM_HERE, technique->_vertex_size * technique->_vertices.size(), &technique->_vertices[0]);
}

void TechniqueParser::parse_indices(GraphicsInterface *graphics, Scope *scope, Technique *technique) {

	auto valid_tags = vector<Symbol>(list_of(kSymFormat)(kSymData));
	auto index_fmts = map_list_of("index16", DXGI_FORMAT_R16_UINT)("index32", DXGI_FORMAT_R32_UINT);

	technique->_index_format = DXGI_FORMAT_UNKNOWN;
	Symbol symbol;

	while (scope->consume_in(valid_tags, &symbol)) {

		switch (symbol) {

			case kSymFormat: {
				string format = scope->string_until(';');
				technique->_index_format = lookup_throw<string, DXGI_FORMAT>(format, index_fmts);
				scope->munch(kSymSemicolon);
				break;
			}

			case kSymData: {
				Scope inner = scope->create_delimited_scope('[', ']');
				scope->advance(inner).munch(kSymSemicolon);

				vector<vector<int>> indices_outer;
				parse_list<int>(&inner, &indices_outer, [&](const string &x){ return parse_int(x); });
				THROW_ON_FALSE(indices_outer.size() == 1);
				const vector<int> &indices = indices_outer[0];

				int max_value = INT_MIN;
				for (auto it = begin(indices); it != end(indices); ++it) {
          const int v = *it;
          max_value = max(max_value, v);
          technique->_indices.push_back(v);
        }

				if (technique->_index_format == DXGI_FORMAT_UNKNOWN)
					technique->_index_format = max_value < (1 << 16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

				break;
			}
		}
	}

	THROW_ON_FALSE(technique->_index_format != DXGI_FORMAT_UNKNOWN);
	if (technique->_index_format == DXGI_FORMAT_R16_UINT) {
		// create a local copy of the indices
		vector<uint16> v;
		copy(technique->_indices.begin(), technique->_indices.end(), back_inserter(v));
		technique->_ib = graphics->create_static_index_buffer(FROM_HERE, index_format_to_size(technique->_index_format) * v.size(), &v[0]);
	} else {
		technique->_ib = graphics->create_static_index_buffer(FROM_HERE, index_format_to_size(technique->_index_format) * technique->_indices.size(), &technique->_indices[0]);
	}
}

void TechniqueParser::parse_technique(GraphicsInterface *graphics, Scope *scope, Technique *technique) {

	auto valid = list_of
		(kSymVertexShader)(kSymPixelShader)
		(kSymMaterial)
		(kSymVertices)(kSymIndices)
		(kSymRasterizerDesc)(kSymSamplerDesc)(kSymDepthStencilDesc)(kSymBlendDesc);

	Symbol symbol;
	while (!scope->end() && scope->consume_in(valid, &symbol)) {
		switch (symbol) {

			case kSymVertexShader:
			case kSymPixelShader: {
				bool vs = symbol == kSymVertexShader;
				Rollback2<Shader> shader;
				if (vs) 
					shader.set(new VertexShader()); 
				else 
					shader.set(new PixelShader());
				scope->munch(kSymBlockOpen);
				parse_shader(scope, shader.get());
				*(vs ? &technique->_vertex_shader : &technique->_pixel_shader) = shader.commit();
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;
			}

			case kSymMaterial: {
				string technique_name = technique->name();
				// materials are prefixed by the technique name
				Rollback2<Material> mat(new Material(technique_name + "::" + scope->next_identifier()));
				scope->munch(kSymBlockOpen);
				parse_material(scope, mat.get());
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				technique->_materials.push_back(mat.commit());
				break;
			}

			case kSymRasterizerDesc:
				scope->munch(kSymBlockOpen);
				parse_rasterizer_desc(scope, &technique->_rasterizer_desc);
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;

			case kSymSamplerDesc: {
				string name = scope->next_identifier();
        if (scope->consume_if(kSymDefault)) {
          technique->_default_sampler_state = name;
        }
				scope->munch(kSymBlockOpen);

				D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
				parse_sampler_desc(scope, &desc);
				technique->_sampler_descs.push_back(make_pair(name, desc));
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;
			}

			case kSymDepthStencilDesc:
				scope->munch(kSymBlockOpen);
				parse_depth_stencil_desc(scope, &technique->_depth_stencil_desc);
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;

			case kSymBlendDesc:
				scope->munch(kSymBlockOpen);
				parse_blend_desc(scope, &technique->_blend_desc);
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;

			case kSymVertices:
				scope->munch(kSymBlockOpen);
				parse_vertices(graphics, scope, technique);
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;

			case kSymIndices:
				scope->munch(kSymBlockOpen);
				parse_indices(graphics, scope, technique);
				scope->munch(kSymBlockClose).munch(kSymSemicolon);
				break;
		}
	}
}

bool TechniqueParser::parse(GraphicsInterface *graphics, const char *start, const char *end, vector<Technique *> *techniques, vector<Material *> *materials) {

	try {
		Scope scope(this, start, end-1);

		while (!scope.end()) {

			switch (scope.next_symbol()) {

				case kSymTechnique: {
          unique_ptr<Technique> t(new Technique);
					scope.next_identifier(&t.get()->_name).munch(kSymBlockOpen);
					parse_technique(graphics, &scope, t.get());
					scope.munch(kSymBlockClose).munch(kSymSemicolon);
					techniques->push_back(t.release());
					break;
				}

				default:
					return false;
			}
		}
	} catch (const parser_exception &/*e*/) {
		return false;
	}

	if (materials) {
		materials->clear();
		for (auto it = std::begin(*techniques); it != std::end(*techniques); ++it)
		copy(std::begin((*it)->_materials), std::end((*it)->_materials), back_inserter(*materials));
	}

	return true;
}
