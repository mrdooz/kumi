#include "stdafx.h"
#include "technique_parser.hpp"
#include "technique.hpp"
#include "string_utils.hpp"
#include "file_utils.hpp"
#include "property.hpp"
#include "material.hpp"
#include "logger.hpp"
#include "tracked_location.hpp"
#include "path_utils.hpp"
#include "graphics.hpp"
#include "resource_interface.hpp"

using namespace std;
using namespace boost::assign;
using namespace tr1::placeholders;

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
  kSymInclude,
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
  kSymInherits,
  kSymId,
  kSymVertexShader,
  kSymPixelShader,
  kSymComputeShader,
  kSymGeometryShader,
    kSymFile,
    kSymEntryPoint,
    kSymParams,
    kSymFlags,

  kSymVertices,
    kSymFormat,
    kSymData,
  kSymIndices,
  kSymGeometry,

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
  { kSymInclude, "include" },
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
  { kSymInherits, "inherits" },

  { kSymVertexShader, "vertex_shader" },
  { kSymPixelShader, "pixel_shader" },
  { kSymComputeShader, "compute_shader" },
  { kSymGeometryShader, "geometry_shader" },
    { kSymFile, "file" },
    { kSymEntryPoint, "entry_point" },
    { kSymParams, "params" },
    { kSymFlags, "flags" },

  { kSymMaterial, "material" },

  { kSymGeometry, "geometry" },

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
  (kSymInt, PropertyType::kInt)
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
    KASSERT(cur->symbol == kSymUnknown);
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

TechniqueParser::TechniqueParser(const std::string &filename, TechniqueFile *result) 
  : _symbol_trie(new Trie()) 
  , _result(result)
  , _filename(RESOURCE_MANAGER.resolve_filename(filename.c_str(), true))
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

    LOG_ERROR_DIRECT_LN(_parser->_filename.c_str(), line, "Expected: \"%s\", found \"%s\"", 
      expected_symbol,
      string(_start, min(_end - _start + 1, 20)).c_str());
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

void TechniqueParser::parse_param(const vector<string> &param, Technique *technique, ShaderTemplate *shader) {

  auto valid_sources = map_list_of
    ("material", PropertySource::kMaterial)
    ("system", PropertySource::kSystem)
    ("instance", PropertySource::kInstance)
    ("user", PropertySource::kUser)
    ("mesh", PropertySource::kMesh);

  THROW_ON_FALSE(param.size() >= 3);

  // type, name and source are required. the default value is optional
  Symbol sym_type = _symbol_trie->find_symbol(param[0]);
  PropertyType::Enum type = lookup_throw<Symbol, PropertyType::Enum>(sym_type, valid_property_types);

  // check if the property is an array
  const char *open_bracket = strchr(param[0].c_str(), '[');
  const char *close_bracket = open_bracket ? strchr(open_bracket+1, ']') : nullptr;
  int len = open_bracket && close_bracket ? atoi(open_bracket + 1) : 0;
  type = (PropertyType::Enum)MAKELONG(type, len);

  string name = param[1];
  string friendly_name = param.size() == 4 ? param[2] : "";
  int src_pos = friendly_name.empty() ? 2 : 3;
  PropertySource::Enum source = lookup_throw<string, PropertySource::Enum>(param[src_pos], valid_sources);

  bool cbuffer_param = false;
  if (type == PropertyType::kTexture2d) {
    shader->_resource_view_params.push_back(ResourceViewParam(name, type, source, friendly_name));
  } else {
    shader->_cbuffer_params.push_back(CBufferParam(name, type, source));
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
    } else if (isspace((int)ch)) {
      cur.push_back(xform(string(item_start, scope->_start - item_start - 1)));
      item_start = scope->skip_whitespace()._start;
    }
  }

  if (item_start < scope->_end)
    cur.push_back(xform(string(item_start, scope->_start - item_start)));

  if (!cur.empty())
    items->push_back(cur);

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

static const char *ext_from_type(ShaderType::Enum type) {
  switch (type) {
    case ShaderType::kVertexShader: return ".vso";
    case ShaderType::kPixelShader: return ".pso";
    case ShaderType::kComputeShader: return ".cso";
    case ShaderType::kGeometryShader: return ".gso";
    default: LOG_ERROR_LN("Implement me!");
  }
  __assume(false);
}

void TechniqueParser::parse_shader_template(Scope *scope, Technique *technique, ShaderTemplate *shader) {

  auto tmp = list_of(kSymFile)(kSymEntryPoint)(kSymParams)(kSymFlags);
  string ext = ext_from_type(shader->_type);
  Symbol symbol;

  while (scope->consume_in(tmp, &symbol)) {

    switch (symbol) {

      case kSymFile: {
        shader->_source_filename = scope->string_until(';');
        // Use the default entry point name until we get a real tag
        shader->_obj_filename = strip_extension(shader->_source_filename) + "_" + shader->_entry_point + ext;
        scope->munch(kSymSemicolon);
        break;
      }

      case kSymEntryPoint: {
        shader->_entry_point = scope->string_until(';');
        // update the obj filename
        if (!shader->_source_filename.empty())
          shader->_obj_filename = strip_extension(shader->_source_filename) + "_" + shader->_entry_point + ext;
        scope->munch(kSymSemicolon);
        break;
      }

      case kSymParams : {
        Scope inner = scope->create_delimited_scope('[', ']');
        vector<vector<string>> params;
        parse_list(&inner, &params);
        for (auto it = begin(params); it != end(params); ++it)
          parse_param(*it, technique, shader);
        scope->advance(inner).munch(kSymSemicolon);
        break;
      }

      case kSymFlags: {
        Scope inner = scope->create_delimited_scope('[', ']');
        vector<vector<string>> flags;
        parse_list(&inner, &flags);
        for (size_t i = 0; i < flags.size(); ++i) {
          const std::string &flag = flags[i][0];
          GRAPHICS.add_shader_flag(flag);
          shader->_flags.push_back(flag);
          // update the shader flag mask
          int flag_value = GRAPHICS.get_shader_flag(flag);
          if (shader->_type == ShaderType::kVertexShader)
            technique->_vs_flag_mask |= flag_value;
          else if (shader->_type == ShaderType::kPixelShader)
            technique->_ps_flag_mask |= flag_value;
          else if (shader->_type == ShaderType::kComputeShader)
            technique->_cs_flag_mask |= flag_value;
          else if (shader->_type == ShaderType::kGeometryShader)
            technique->_gs_flag_mask |= flag_value;
          else
            LOG_ERROR_LN("Implement me!");
        }
        scope->advance(inner).munch(kSymSemicolon);
        break;
      }
    }
  }

  LOG_ERROR_COND_LN(!shader->_entry_point.empty(), "No shader entry point given!");
}

void TechniqueParser::parse_depth_stencil_desc(Scope *scope, CD3D11_DEPTH_STENCIL_DESC *desc) {

  *desc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());

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

void TechniqueParser::parse_sampler_desc(Scope *scope, CD3D11_SAMPLER_DESC *desc) {

  *desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());

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

  auto valid_lhs = list_of
    (kSymBlendEnable)(kSymSrcBlend)(kSymDestBlend)(kSymSrcBlendAlpha)
    (kSymDestBlendAlpha)(kSymBlendOp)(kSymBlendOpAlpha)(kSymRenderTargetWriteMask);

  auto valid_blend_settings = map_list_of
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
  while (!scope->end() && scope->consume_in(valid_lhs, &symbol)) {
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
        lookup<string, D3D11_BLEND>(value, valid_blend_settings,
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

void TechniqueParser::parse_blend_desc(Scope *scope, CD3D11_BLEND_DESC *desc) {

  *desc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());

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

void TechniqueParser::parse_rasterizer_desc(Scope *scope, CD3D11_RASTERIZER_DESC *desc) {

  *desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());

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

void TechniqueParser::parse_vertices(Scope *scope, Technique *technique) {

  auto valid_tags = vector<Symbol>(list_of(kSymFormat)(kSymData));
  auto vertex_fmts = map_list_of("pos", 3 * sizeof(float))("pos_tex", 5 * sizeof(float));
  vector<float> vertices;

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
            vertices.push_back(*jt);
          }
        }
        break;
      }
    }
  }
  THROW_ON_FALSE(technique->_vertex_size != -1);
  technique->_vb = GRAPHICS.create_static_vertex_buffer(FROM_HERE, technique->_vertex_size * vertices.size(), &vertices[0]);
}

void TechniqueParser::parse_indices(Scope *scope, Technique *technique) {

  auto valid_tags = vector<Symbol>(list_of(kSymFormat)(kSymData));
  auto index_fmts = map_list_of("index16", DXGI_FORMAT_R16_UINT)("index32", DXGI_FORMAT_R32_UINT);

  technique->_index_format = DXGI_FORMAT_UNKNOWN;
  Symbol symbol;
  vector<int> indices;

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
        const vector<int> &indices_inner = indices_outer[0];

        int max_value = INT_MIN;
        for (auto it = begin(indices_inner); it != end(indices_inner); ++it) {
          const int v = *it;
          max_value = max(max_value, v);
          indices.push_back(v);
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
    copy(RANGE(indices), back_inserter(v));
    technique->_ib = GRAPHICS.create_static_index_buffer(FROM_HERE, index_format_to_size(technique->_index_format) * v.size(), &v[0]);
  } else {
    technique->_ib = GRAPHICS.create_static_index_buffer(FROM_HERE, index_format_to_size(technique->_index_format) * indices.size(), &indices[0]);
  }
  technique->_index_count = indices.size();
}

void TechniqueParser::parse_technique(Scope *scope, Technique *technique) {

  auto valid = list_of
    (kSymVertexShader)(kSymPixelShader)(kSymComputeShader)(kSymGeometryShader)
    (kSymMaterial)
    (kSymVertices)(kSymIndices)
    (kSymGeometry)
    (kSymRasterizerDesc)(kSymSamplerDesc)(kSymDepthStencilDesc)(kSymBlendDesc);

  Symbol symbol;
  while (!scope->end() && scope->consume_in(valid, &symbol)) {
    switch (symbol) {

      case kSymVertexShader:
      case kSymPixelShader:
      case kSymGeometryShader:
      case kSymComputeShader: {
        ShaderTemplate *st = nullptr;
        if (symbol == kSymVertexShader) 
          technique->_vs_shader_template.reset(st = new ShaderTemplate(ShaderType::kVertexShader, _filename));
        else if (symbol == kSymPixelShader)
          technique->_ps_shader_template.reset(st = new ShaderTemplate(ShaderType::kPixelShader, _filename));
        else if (symbol == kSymComputeShader)
          technique->_cs_shader_template.reset(st = new ShaderTemplate(ShaderType::kComputeShader, _filename));
        else if (symbol == kSymGeometryShader)
          technique->_gs_shader_template.reset(st = new ShaderTemplate(ShaderType::kGeometryShader, _filename));
        else
          LOG_ERROR_LN("Implement me!");

        scope->munch(kSymBlockOpen);
        parse_shader_template(scope, technique, st);
        scope->munch(kSymBlockClose).munch(kSymSemicolon);

        break;
      }

      case kSymMaterial: {
        string technique_name = technique->name();
        // materials are prefixed by the technique name
        unique_ptr<Material> mat(new Material(technique_name + "::" + scope->next_identifier()));
        scope->munch(kSymBlockOpen);
        parse_material(scope, mat.get());
        scope->munch(kSymBlockClose).munch(kSymSemicolon);
        technique->_materials.push_back(mat.release());
        break;
      }

      case kSymRasterizerDesc: {
        Symbol next = scope->peek();
        if (next == kSymEquals) {
          // find the rasterizer desc being referenced
          string name;
          scope->munch(kSymEquals).next_identifier(&name).munch(kSymSemicolon);
          auto it = _result->rasterizer_states.find(name);
          THROW_ON_FALSE(it != _result->rasterizer_states.end());
          technique->_rasterizer_state = it->second;
          
        } else if (next == kSymBlockOpen) {
          scope->munch(kSymBlockOpen);
          CD3D11_RASTERIZER_DESC desc;
          parse_rasterizer_desc(scope, &desc);
          technique->_rasterizer_state = GRAPHICS.create_rasterizer_state(FROM_HERE, desc);
          scope->munch(kSymBlockClose).munch(kSymSemicolon);
        } else {
          THROW_ON_FALSE(!"Unknown symbol");
        }
        break;
      }

      case kSymDepthStencilDesc: {
        Symbol next = scope->peek();
        if (next == kSymEquals) {
          string name;
          scope->munch(kSymEquals).next_identifier(&name).munch(kSymSemicolon);
          auto it = _result->depth_stencil_states.find(name);
          THROW_ON_FALSE(it != _result->depth_stencil_states.end());
          technique->_depth_stencil_state = it->second;

        } else if (next == kSymBlockOpen) {
          scope->munch(kSymBlockOpen);
          CD3D11_DEPTH_STENCIL_DESC desc;
          parse_depth_stencil_desc(scope, &desc);
          technique->_depth_stencil_state = GRAPHICS.create_depth_stencil_state(FROM_HERE, desc);
          scope->munch(kSymBlockClose).munch(kSymSemicolon);
        } else {
          THROW_ON_FALSE(!"Unknown symbol");
        }

        break;
      }

      case kSymBlendDesc: {
        Symbol next = scope->peek();
        if (next == kSymEquals) {
          string name;
          scope->munch(kSymEquals).next_identifier(&name).munch(kSymSemicolon);
          auto it = _result->blend_states.find(name);
          THROW_ON_FALSE(it != _result->blend_states.end());
          technique->_blend_state = it->second;

        } else if (next == kSymBlockOpen) {
          scope->munch(kSymBlockOpen);
          CD3D11_BLEND_DESC desc;
          parse_blend_desc(scope, &desc);
          technique->_blend_state = GRAPHICS.create_blend_state(FROM_HERE, desc);
          scope->munch(kSymBlockClose).munch(kSymSemicolon);
        } else {
          THROW_ON_FALSE(!"Unknown symbol");
        }
        break;
      }

      // sampler descs don't belong in the technique..
/*
      case kSymSamplerDesc: {
        string name = scope->next_identifier();
        scope->munch(kSymBlockOpen);

        CD3D11_SAMPLER_DESC desc;
        parse_sampler_desc(scope, &desc);
        technique->_sampler_descs.push_back(make_pair(name, desc));
        scope->munch(kSymBlockClose).munch(kSymSemicolon);
        break;
      }
*/

      case kSymVertices:
        scope->munch(kSymBlockOpen);
        parse_vertices(scope, technique);
        scope->munch(kSymBlockClose).munch(kSymSemicolon);
        break;

      case kSymIndices:
        scope->munch(kSymBlockOpen);
        parse_indices(scope, technique);
        scope->munch(kSymBlockClose).munch(kSymSemicolon);
        break;

      case kSymGeometry: {
        scope->munch(kSymEquals);
        string geom_name = scope->string_until(';');
        Graphics::PredefinedGeometry geom;
        lookup<string, Graphics::PredefinedGeometry>(geom_name,
          map_list_of
          ("fs_quad_postex", Graphics::kGeomFsQuadPosTex)
          ("fs_quad_pos", Graphics::kGeomFsQuadPos),
          &geom);
        GRAPHICS.get_predefined_geometry(geom, &technique->_vb, &technique->_vertex_size, &technique->_ib, &technique->_index_format, &technique->_index_count);
        scope->munch(kSymSemicolon);
        break;
      }
    }
  }
}

template<typename Desc>
void parse_standalone_desc(Scope *scope,
                           function<void(Scope *, Desc *)> parser,
                           function<GraphicsObjectHandle(Desc)> creator,
                           map<string, GraphicsObjectHandle> *states) {
  string name;
  scope->next_identifier(&name).munch(kSymBlockOpen);
  auto it = states->find(name);
  if (states->find(name) != states->end()) {
    LOG_WARNING_LN("Duplicate state found: %s", name.c_str());
  }
  Desc desc;
  parser(scope, &desc);
  scope->munch(kSymBlockClose).munch(kSymSemicolon);
  (*states)[name] = creator(desc);
}

Technique *TechniqueParser::find_technique(const std::string &str) {
  for (size_t i = 0; i < _result->techniques.size(); ++i) {
    if (_result->techniques[i]->name() == str) {
      return _result->techniques[i];
    }
  }
  return nullptr;
}

bool TechniqueParser::parse() {

  vector<char> buf;
  B_ERR_BOOL(RESOURCE_MANAGER.load_file(_filename.c_str(), &buf));

  try {
    Scope scope(this, buf.data(), buf.data() + buf.size() - 1);

    while (!scope.end()) {

      switch (scope.next_symbol()) {

        case kSymInclude: {
          string filename = scope.string_until(';');
          string path;
          if (Path::is_absolute(_filename)) {
            path = Path::get_path(_filename);
          } else {
            char buf[MAX_PATH+1];
            string cwd(_getcwd(buf, MAX_PATH));
            path = Path::concat(cwd, Path::get_path(_filename));
          }
          scope.munch(kSymSemicolon);
          TechniqueParser inner(Path::concat(path, filename), _result);
          if (!inner.parse())
            return false;
          break;
        }
       
        case kSymRasterizerDesc: {
          parse_standalone_desc<CD3D11_RASTERIZER_DESC>(&scope,
            bind(&TechniqueParser::parse_rasterizer_desc, this, _1, _2),
            bind(&Graphics::create_rasterizer_state, &GRAPHICS, FROM_HERE, _1),
            &_result->rasterizer_states);
          break;
        }

        case kSymDepthStencilDesc: {
          parse_standalone_desc<CD3D11_DEPTH_STENCIL_DESC>(&scope, 
            bind(&TechniqueParser::parse_depth_stencil_desc, this, _1, _2),
            bind(&Graphics::create_depth_stencil_state, &GRAPHICS, FROM_HERE, _1),
            &_result->depth_stencil_states);
          break;
        }

        case kSymBlendDesc: {
          parse_standalone_desc<CD3D11_BLEND_DESC>(&scope, 
            bind(&TechniqueParser::parse_blend_desc, this, _1, _2),
            bind(&Graphics::create_blend_state, &GRAPHICS, FROM_HERE, _1),
            &_result->blend_states);
          break;
        }

        case kSymSamplerDesc: {
          string name;
          scope.next_identifier(&name).munch(kSymBlockOpen);
          auto it = _result->sampler_states.find(name);
          if (_result->sampler_states.find(name) != _result->sampler_states.end()) {
            LOG_WARNING_LN("Duplicate state found: %s", name.c_str());
          }
          CD3D11_SAMPLER_DESC desc;
          parse_sampler_desc(&scope, &desc);
          scope.munch(kSymBlockClose).munch(kSymSemicolon);
          _result->sampler_states[name] = GRAPHICS.create_sampler(FROM_HERE, name, desc);
          break;
        }

        case kSymTechnique: {
          unique_ptr<Technique> t(new Technique);
          scope.next_identifier(&t.get()->_name);
          if (scope.consume_if(kSymBlockOpen)) {
          } else if (scope.consume_if(kSymInherits)) {
            string parent;
            scope.next_identifier(&parent).munch(kSymBlockOpen);
            Technique *tt = find_technique(parent);
            if (!tt) {
              LOG_ERROR_LN("Unknown parent: %s", parent.c_str());
              return false;
            }
            t->init_from_parent(tt);
          }
          parse_technique(&scope, t.get());
          _result->techniques.push_back(t.release());
          scope.munch(kSymBlockClose).munch(kSymSemicolon);
          break;
        }

        default:
          return false;
      }
    }
  } catch (const parser_exception &/*e*/) {
    return false;
  }

  for (auto it = begin(_result->techniques); it != end(_result->techniques); ++it)
    copy(RANGE((*it)->_materials), back_inserter(_result->materials));

  return true;
}
