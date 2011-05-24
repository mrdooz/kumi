#include "stdafx.h"
#include "d3d_parser.hpp"

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

using qi::eps;
using qi::lit;
using qi::_val;
using qi::int_;
using qi::uint_;
using qi::float_;
using phoenix::bind;
using ascii::char_;
using std::vector;

static string from_vector(const vector<char>& v)
{
	string s;
	s.resize(v.size());
	std::copy(v.begin(), v.end(), s.begin());
	return s;
}

// D3D symbol tables
struct d3d11_bool_ : qi::symbols<char, BOOL> {
	d3d11_bool_() {
		add
			("TRUE", TRUE)
			("FALSE", FALSE)
			;
	}
} d3d11_bool;

struct d3d11_blend_op_ : qi::symbols<char, D3D11_BLEND_OP> {
#define MK_TAG(x) ("BLEND_OP_" #x, D3D11_BLEND_OP_ ## x)
	d3d11_blend_op_() {
		add
			MK_TAG(ADD)
			MK_TAG(SUBTRACT)
			MK_TAG(REV_SUBTRACT)
			MK_TAG(MIN)
			MK_TAG(MAX)
			;
	}
#undef MK_TAG
} d3d11_blend_op;

struct d3d11_blend_ : qi::symbols<char, D3D11_BLEND> {
#define MK_TAG(x) ("BLEND_" #x, D3D11_BLEND_ ## x)
	d3d11_blend_() {
		add
			MK_TAG(ZERO)
			MK_TAG(ONE)
			MK_TAG(SRC_COLOR)
			MK_TAG(INV_SRC_COLOR)
			MK_TAG(SRC_ALPHA)
			MK_TAG(INV_SRC_ALPHA)
			MK_TAG(DEST_ALPHA)
			MK_TAG(INV_DEST_ALPHA)
			MK_TAG(DEST_COLOR)
			MK_TAG(INV_DEST_COLOR)
			MK_TAG(SRC_ALPHA_SAT)
			MK_TAG(BLEND_FACTOR)
			MK_TAG(SRC1_COLOR)
			MK_TAG(INV_SRC1_COLOR)
			MK_TAG(SRC1_ALPHA)
			MK_TAG(INV_SRC1_ALPHA)
			;
	}
#undef MK_TAG
} d3d11_blend;

struct d3d11_fill_mode_ : qi::symbols<char, D3D11_FILL_MODE> {
#define MK_TAG(x) ("FILL_" #x, D3D11_FILL_ ## x)
	d3d11_fill_mode_() {
		add
			MK_TAG(WIREFRAME)
			MK_TAG(SOLID)
			;
	}
#undef MK_TAG
} d3d11_fill_mode;

struct d3d11_cull_mode_ : qi::symbols<char, D3D11_CULL_MODE> {
#define MK_TAG(x) ("CULL_" #x, D3D11_CULL_ ## x)
	d3d11_cull_mode_() {
		add
			MK_TAG(NONE)
			MK_TAG(FRONT)
			MK_TAG(BACK)
			;
	}
#undef MK_TAG
} d3d11_cull_mode;

struct d3d11_depth_write_mask_ : qi::symbols<char, D3D11_DEPTH_WRITE_MASK> {
#define MK_TAG(x) ("DEPTH_WRITE_MASK_" #x, D3D11_DEPTH_WRITE_MASK_ ## x)
	d3d11_depth_write_mask_() {
		add
			MK_TAG(ZERO)
			MK_TAG(ALL)
			;
	}
#undef MK_TAG
} d3d11_depth_write_mask;

struct d3d11_comparison_func_ : qi::symbols<char, D3D11_COMPARISON_FUNC> {
#define MK_TAG(x) ("COMPARISON_" #x, D3D11_COMPARISON_ ## x)
	d3d11_comparison_func_ () {
		add
			MK_TAG(NEVER)
			MK_TAG(LESS)
			MK_TAG(EQUAL)
			MK_TAG(LESS_EQUAL)
			MK_TAG(GREATER)
			MK_TAG(NOT_EQUAL)
			MK_TAG(GREATER_EQUAL)
			MK_TAG(ALWAYS)
			;
	}
#undef MK_TAG
} d3d11_comparison_func;

struct d3d11_stencil_op_ : qi::symbols<char, D3D11_STENCIL_OP> {
#define MK_TAG(x) ("STENCIL_OP_" #x, D3D11_STENCIL_OP_ ## x)
	d3d11_stencil_op_ () {
		add
			MK_TAG(KEEP)
			MK_TAG(ZERO)
			MK_TAG(REPLACE)
			MK_TAG(INCR_SAT)
			MK_TAG(DECR_SAT)
			MK_TAG(INVERT)
			MK_TAG(INCR)
			MK_TAG(DECR)
			;
	}
#undef MK_TAG
} d3d11_stencil_op;

void set_rt_defaults(D3D11_RENDER_TARGET_BLEND_DESC *desc)
{
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		FALSE,
		D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	*desc = defaultRenderTargetBlendDesc;
}

void set_depth_stencil_op_defaults(D3D11_DEPTH_STENCILOP_DESC *desc)
{
	const D3D11_DEPTH_STENCILOP_DESC defaultStencilOp =
	{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS };
	*desc = defaultStencilOp;

}

void set_rt_blend_desc(D3D11_BLEND_DESC *blend_desc, int i, const D3D11_RENDER_TARGET_BLEND_DESC &rt_blend_desc)
{
	if (i < 0 || i >= 8)
		return;

	blend_desc->RenderTarget[i] = rt_blend_desc;
}
#if 0
template <typename Iterator>
struct blend_desc_parser_ : qi::grammar<Iterator, D3D11_BLEND_DESC(), ascii::space_type>
{
	blend_desc_parser_() : blend_desc_parser_::base_type(start)
	{
		start =
			eps[qi::_val = CD3D11_BLEND_DESC(CD3D11_DEFAULT())] >> 
			lit("BlendDesc") >> '{' >> 
			*(			
			(lit("AlphaToCoverageEnable") >> '=' >> d3d11_bool >> ';')                 [bind(&D3D11_BLEND_DESC::AlphaToCoverageEnable, _val) = qi::_1]
		|| (lit("IndependentBlendEnable") >> '=' >> d3d11_bool >> ';')               [bind(&D3D11_BLEND_DESC::IndependentBlendEnable, _val) = qi::_1]
		|| ((lit("RenderTarget") >> '[' >> int_ >> ']') >> '=' >> '{' >> rt_blend_rule >> '}' >> ';')     [bind(&set_rt_blend_desc, &qi::_val, qi::_1, qi::_2)]
		)
			>> '}' >> ';'
			;

		rt_blend_rule =
			eps[bind(&set_rt_defaults, &qi::_val)] >> 
			*(
			(lit("BlendEnable") >> '=' >> d3d11_bool >> ';')         [bind(&D3D11_RENDER_TARGET_BLEND_DESC::BlendEnable, _val) = qi::_1]
		|| (lit("SrcBlend") >> '=' >> d3d11_blend >> ';')          [bind(&D3D11_RENDER_TARGET_BLEND_DESC::SrcBlend, _val) = qi::_1]
		|| (lit("DestBlend") >> '=' >> d3d11_blend >> ';')         [bind(&D3D11_RENDER_TARGET_BLEND_DESC::DestBlend, _val) = qi::_1]
		|| (lit("BlendOp") >> '=' >> d3d11_blend_op >> ';')        [bind(&D3D11_RENDER_TARGET_BLEND_DESC::BlendOp, _val) = qi::_1]
		|| (lit("SrcBlendAlpha") >> '=' >> d3d11_blend >> ';')     [bind(&D3D11_RENDER_TARGET_BLEND_DESC::SrcBlendAlpha, _val) = qi::_1]
		|| (lit("DestBlendAlpha") >> '=' >> d3d11_blend >> ';')    [bind(&D3D11_RENDER_TARGET_BLEND_DESC::DestBlendAlpha, _val) = qi::_1]
		|| (lit("BlendOpAlpha") >> '=' >> d3d11_blend_op >> ';')   [bind(&D3D11_RENDER_TARGET_BLEND_DESC::BlendOpAlpha, _val) = qi::_1]
		|| (lit("RenderTargetWriteMask") >> '=' >> int_ >> ';')    [bind(&D3D11_RENDER_TARGET_BLEND_DESC::RenderTargetWriteMask, _val) = qi::_1]
		)
			;
	}

	qi::rule<Iterator, D3D11_RENDER_TARGET_BLEND_DESC(), ascii::space_type> rt_blend_rule;
	qi::rule<Iterator, D3D11_BLEND_DESC(), ascii::space_type> start;
};

template <typename Iterator>
struct depth_stencil_desc_parser_ : qi::grammar<Iterator, D3D11_DEPTH_STENCIL_DESC(), ascii::space_type>
{
	depth_stencil_desc_parser_() : depth_stencil_desc_parser_::base_type(start)
	{
		start =
			eps[qi::_val = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT())] >>
			lit("DepthStencilDesc") >> '{' >>
			*(
			(lit("DepthEnable") >> '=' >> d3d11_bool >> ';')                          [bind(&D3D11_DEPTH_STENCIL_DESC::DepthEnable, _val) = qi::_1]
		|| (lit("DepthWriteMask") >> '=' >> d3d11_depth_write_mask >> ';')          [bind(&D3D11_DEPTH_STENCIL_DESC::DepthWriteMask, _val) = qi::_1]
		|| (lit("DepthFunc") >> '=' >> d3d11_comparison_func >> ';')                [bind(&D3D11_DEPTH_STENCIL_DESC::DepthFunc, _val) = qi::_1]
		|| (lit("StencilEnable") >> '=' >> d3d11_bool >> ';')                       [bind(&D3D11_DEPTH_STENCIL_DESC::StencilEnable, _val) = qi::_1]
		|| (lit("StencilReadMask") >> '=' >> uint_ >> ';')                          [bind(&D3D11_DEPTH_STENCIL_DESC::StencilReadMask, _val) = qi::_1]
		|| (lit("StencilWriteMask") >> '=' >> uint_ >> ';')                         [bind(&D3D11_DEPTH_STENCIL_DESC::StencilWriteMask, _val) = qi::_1]
		|| (lit("FrontFace") >> '=' >> '{' >> depth_stencil_op_desc >> '}' >> ';')  [bind(&D3D11_DEPTH_STENCIL_DESC::FrontFace, _val) = qi::_1]
		|| (lit("BackFace") >> '=' >> '{' >> depth_stencil_op_desc >> '}' >> ';')   [bind(&D3D11_DEPTH_STENCIL_DESC::BackFace, _val) = qi::_1]
		)
			>> '}' >> ';'
			;

		depth_stencil_op_desc =
			eps[bind(&set_depth_stencil_op_defaults, &qi::_val)] >> 
			*(
			(lit("StencilFailOp") >> '=' >> d3d11_stencil_op >> ';')        [bind(&D3D11_DEPTH_STENCILOP_DESC::StencilFailOp, _val) = qi::_1]
		|| (lit("StencilDepthFailOp") >> '=' >> d3d11_stencil_op >> ';')  [bind(&D3D11_DEPTH_STENCILOP_DESC::StencilDepthFailOp, _val) = qi::_1]
		|| (lit("StencilPassOp") >> '=' >> d3d11_stencil_op >> ';')       [bind(&D3D11_DEPTH_STENCILOP_DESC::StencilPassOp, _val) = qi::_1]
		|| (lit("StencilFunc") >> '=' >> d3d11_comparison_func >> ';')    [bind(&D3D11_DEPTH_STENCILOP_DESC::StencilFunc, _val) = qi::_1]
		)
			;

	}
	qi::rule<Iterator, D3D11_DEPTH_STENCILOP_DESC(), ascii::space_type> depth_stencil_op_desc;
	qi::rule<Iterator, D3D11_DEPTH_STENCIL_DESC(), ascii::space_type> start;
};
#endif
template <typename Iterator>
struct rasterizer_desc_parser_ : qi::grammar<Iterator, D3D11_RASTERIZER_DESC(), ascii::space_type>
{
	rasterizer_desc_parser_() : rasterizer_desc_parser_::base_type(start)
	{
		start =
			eps[qi::_val = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT())] >>
			((lit("RasterizerDesc") >> -((+(qi::char_ - '='))[phoenix::ref(name) = qi::_1 ] >> '=') >> '{') || (lit("RasterizerDesc") >> '{'))  >>
			*(
			(lit("FillMode") >> '=' >> d3d11_fill_mode >> ';')               [bind(&D3D11_RASTERIZER_DESC::FillMode, _val) = qi::_1]
		|| (lit("CullMode") >> '=' >> d3d11_cull_mode >> ';')              [bind(&D3D11_RASTERIZER_DESC::CullMode, _val) = qi::_1]
		|| (lit("FrontCounterClockwise") >> '=' >> d3d11_bool >> ';')      [bind(&D3D11_RASTERIZER_DESC::FrontCounterClockwise, _val) = qi::_1]
		|| (lit("DepthBias") >> '=' >> int_ >> ';')                        [bind(&D3D11_RASTERIZER_DESC::DepthBias, _val) = qi::_1]
		|| (lit("DepthBiasClamp") >> '=' >> float_ >> ';')                 [bind(&D3D11_RASTERIZER_DESC::DepthBiasClamp, _val) = qi::_1]
		|| (lit("SlopeScaledDepthBias") >> '=' >> float_ >> ';')           [bind(&D3D11_RASTERIZER_DESC::SlopeScaledDepthBias, _val) = qi::_1]
		|| (lit("DepthClipEnable") >> '=' >> d3d11_bool >> ';')            [bind(&D3D11_RASTERIZER_DESC::DepthClipEnable, _val) = qi::_1]
		|| (lit("ScissorEnable") >> '=' >> d3d11_bool >> ';')              [bind(&D3D11_RASTERIZER_DESC::ScissorEnable, _val) = qi::_1]
		|| (lit("MultisampleEnable") >> '=' >> d3d11_bool >> ';')          [bind(&D3D11_RASTERIZER_DESC::MultisampleEnable, _val) = qi::_1]
		|| (lit("AntialiasedLineEnable") >> '=' >> d3d11_bool >> ';')      [bind(&D3D11_RASTERIZER_DESC::AntialiasedLineEnable, _val) = qi::_1]
		)
			>> '}' >> ';'
			;
	}
	vector<char> name;
	qi::rule<Iterator, D3D11_RASTERIZER_DESC(), ascii::space_type> start;
};

const char *parse_blend_desc(const char *start, const char *end, D3D11_BLEND_DESC *desc, string *name)
{
	return NULL;
	//return phrase_parse(start, end, blend_desc_parser_<const char *>(), ascii::space, *desc);
}

const char *parse_depth_stencil_desc(const char *start, const char *end, D3D11_DEPTH_STENCIL_DESC *desc, string *name)
{
	return NULL;
	//return phrase_parse(start, end, depth_stencil_desc_parser_<const char *>(), ascii::space, *desc);
}

const char *parse_rasterizer_desc(const char *start, const char *end, D3D11_RASTERIZER_DESC *desc, string *name)
{
	rasterizer_desc_parser_<const char *> p;
	const bool res = phrase_parse(start, end, p, ascii::space, *desc);
	if (name)
		*name = from_vector(p.name);
	return res ? start : NULL;
}

const char *parse_sampler_desc(const char *start, const char *end, D3D11_SAMPLER_DESC *desc, string *name)
{
	return NULL;
}

void parse_descs(const char *start, const char *end, 
	map<string, D3D11_BLEND_DESC> *blend_descs, map<string, D3D11_DEPTH_STENCIL_DESC> *depth_descs, 
	map<string, D3D11_RASTERIZER_DESC> *rasterizer_descs, map<string, D3D11_SAMPLER_DESC> *sampler_descs)
{
	const char *cur = start;
	int i = 0;
	int fail_count = 0;
	string name;
	D3D11_BLEND_DESC blend_desc;
	D3D11_DEPTH_STENCIL_DESC dss_desc;
	D3D11_RASTERIZER_DESC rasterizer_desc;
	D3D11_SAMPLER_DESC sampler_desc;

	while (cur != end && fail_count != 4) {
		switch (i) {
		case 0: 
			if (const char *tmp = parse_blend_desc(cur, end, &blend_desc, &name)) {
				cur = tmp;
				if (blend_descs)
					(*blend_descs)[name] = blend_desc;
				fail_count = 0;
			} else {
				++i;
				++fail_count;
			}
			break;

		case 1:
			if (const char *tmp = parse_depth_stencil_desc(cur, end, &dss_desc, &name)) {
				cur = tmp;
				if (depth_descs)
					(*depth_descs)[name] = dss_desc;
				fail_count = 0;
			} else {
				++i;
				++fail_count;
			}
			break;

		case 2: 
			if (const char *tmp = parse_rasterizer_desc(cur, end, &rasterizer_desc, &name)) {
				cur = tmp;
				if (rasterizer_descs)
					(*rasterizer_descs)[name] = rasterizer_desc;
				fail_count = 0;
			} else {
				++i;
				++fail_count;
			}
			break;

		case 3: 
			if (const char *tmp = parse_sampler_desc(cur, end, &sampler_desc, &name)) {
				cur = tmp;
				if (sampler_descs)
					(*sampler_descs)[name] = sampler_desc;
				fail_count = 0;
			} else {
				i = 0;
				++fail_count;
			}
			break;
		}
	}
}
