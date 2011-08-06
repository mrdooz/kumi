#pragma once

#include "utils.hpp"
#include "property_manager.hpp"
#include "graphics.hpp"

using std::vector;
using std::string;

struct ShaderParam {
	struct Source {
		enum Enum {
			kUnknown,
			kMaterial,
			kSystem,
			kUser,
			kMesh,
		};
	};

	union DefaultValue {
		int _int;
		float _float[16];
	};

	string name;
	Property::Enum type;
	Source::Enum source;
	int cbuffer;
	int start_offset;  // offset in cbuffer
	DefaultValue default_value;
};

struct Shader {
	Shader(const string &entry_point) : entry_point(entry_point) {}
	ShaderParam *find_by_name(const char *name) {
		for (size_t i = 0; i < params.size(); ++i) {
			if (params[i].name == name)
				return &params[i];
		}
		return NULL;
	}

	virtual void set_buffers() = 0;

	string filename;
	string entry_point;
	vector<ShaderParam> params;
	vector<GraphicsObjectHandle> constant_buffers;
	// todo: sort params by cbuffers
};

struct VertexShader : public Shader{
	VertexShader() : Shader("vs_main") {}
	virtual void set_buffers() {
	}
};

struct PixelShader : public Shader {
	PixelShader() : Shader("ps_name") {}
	virtual void set_buffers() {
	}
};


struct Io;

class Technique {
	friend class TechniqueParser;
public:
	Technique(Io *io);
	~Technique();

	static Technique *create_from_file(const char *filename, Io *io);
	const string &name() const { return _name; }
private:
	bool init();
	bool init_shader(Shader *shader, const string &profile, bool vertex_shader);
	bool do_reflection(Shader *shader, bool vertex_shader, void *buf, size_t len);

	Io *_io;
	string _name;
	Shader *_vertex_shader;
	Shader *_pixel_shader;

	GraphicsObjectHandle _input_layout;

	CD3D11_RASTERIZER_DESC _rasterizer_desc;
	CD3D11_SAMPLER_DESC _sampler_desc;
	CD3D11_BLEND_DESC _blend_desc;
	CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;
};
