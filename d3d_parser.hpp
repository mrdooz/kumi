#pragma once

const char *parse_blend_desc(const char *start, const char *end, D3D11_BLEND_DESC *desc, string *name);
const char *parse_depth_stencil_desc(const char *start, const char *end, D3D11_DEPTH_STENCIL_DESC *desc, string *name);
const char *parse_rasterizer_desc(const char *start, const char *end, D3D11_RASTERIZER_DESC *desc, string *name);
const char *parse_sampler_desc(const char *start, const char *end, D3D11_SAMPLER_DESC *desc, string *name);

void parse_descs(const char *start, const char *end, 
	map<string, D3D11_BLEND_DESC> *blend_descs, map<string, D3D11_DEPTH_STENCIL_DESC> *depth_descs, 
	map<string, D3D11_RASTERIZER_DESC> *rasterizer_descs, map<string, D3D11_SAMPLER_DESC> *sampler_descs);
