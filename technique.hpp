#pragma once

using std::vector;
using std::string;

#include "utils.hpp"

struct ShaderParam {
	struct Type {
		enum Enum {
			kUnknown,
			kFloat,
			kFloat2,
			kFloat3,
			kFloat4,
			kInt,
			kMatrix,
		};
	};

	struct Source {
		enum Enum {
			kUnknown,
			kMaterial,
			kSystem,
			kUser,
		};
	};

	union DefaultValue {
		int _int;
		float _float[16];
	};

	string name;
	Type::Enum type;
	Source::Enum source;
	DefaultValue default_value;
};

struct Shader {
	string filename;
	vector<ShaderParam> params;
};

struct VertexShader : public Shader{
};

struct PixelShader : public Shader {
};

class Technique {
	friend class TechniqueParser;
public:
	Technique();
	~Technique();

	static Technique *create_from_file(const char *filename);
private:
	string _name;
	VertexShader *_vertex_shader;
	PixelShader *_pixel_shader;
	CD3D11_RASTERIZER_DESC _rasterizer_desc;
	CD3D11_SAMPLER_DESC _sampler_desc;
	CD3D11_BLEND_DESC _blend_desc;
	CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;
};
