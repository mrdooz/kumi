#pragma once

bool parse_blend_desc(const char *start, const char *end, D3D11_BLEND_DESC *desc);
bool parse_depth_stencil_desc(const char *start, const char *end, D3D11_DEPTH_STENCIL_DESC *desc);
bool parse_rasterizer_desc(const char *start, const char *end, D3D11_RASTERIZER_DESC *desc);