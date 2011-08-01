#include "stdafx.h"
#include "technique.hpp"
#include "technique_parser.hpp"

Technique::Technique()
	: _vertex_shader(nullptr)
	, _pixel_shader(nullptr)
	, _rasterizer_desc(CD3D11_DEFAULT())
	, _sampler_desc(CD3D11_DEFAULT())
	, _blend_desc(CD3D11_DEFAULT())
	, _depth_stencil_desc(CD3D11_DEFAULT())
{
}

Technique::~Technique() {
	SAFE_DELETE(_vertex_shader);
	SAFE_DELETE(_pixel_shader);
}

Technique *Technique::create_from_file(const char *filename) {
	void *buf;
	size_t len;
	if (!load_file(filename, &buf, &len))
		return NULL;


}
