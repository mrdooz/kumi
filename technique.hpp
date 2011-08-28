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
			kTechnique,
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
	int size;
	DefaultValue default_value;
};

struct Shader {
	enum Type {
		kVertexShader,
		kPixelShader,
		kGeometryShader,
	};

	Shader(const string &entry_point) : entry_point(entry_point), valid(false) {}
	ShaderParam *find_by_name(const char *name) {
		for (size_t i = 0; i < params.size(); ++i) {
			if (params[i].name == name)
				return &params[i];
		}
		return NULL;
	}

	virtual void set_buffers() = 0;
	virtual Type type() const = 0;
	bool is_valid() const { return valid; }

	bool valid;
	string filename;
	string source;
	string entry_point;
	vector<ShaderParam> params;
	GraphicsObjectHandle shader;
};

struct VertexShader : public Shader{
	VertexShader() : Shader("vs_main") {}
	virtual void set_buffers() {
	}
	virtual Type type() const { return kVertexShader; }
};

struct PixelShader : public Shader {
	PixelShader() : Shader("ps_main") {}
	virtual void set_buffers() {
	}
	virtual Type type() const { return kPixelShader; }
};


struct Io;

struct CBuffer {
	CBuffer(const string &name, int size, GraphicsObjectHandle handle) 
		: name(name), handle(handle) 
	{
		staging.resize(size);
	}
	string name;
	vector<uint8> staging;
	GraphicsObjectHandle handle;
};

class Technique {
	friend class TechniqueParser;
public:
	Technique();
	~Technique();

	static Technique *create_from_file(const char *filename);
	const string &name() const { return _name; }

	GraphicsObjectHandle input_layout() const { return _input_layout; }
	Shader *vertex_shader() { return _vertex_shader; }
	Shader *pixel_shader() { return _pixel_shader; }

	vector<CBuffer> &get_cbuffers() { return _constant_buffers; }

	GraphicsObjectHandle rasterizer_state() const { return _rasterizer_state; }
	GraphicsObjectHandle sampler_state() const { return _sampler_state; }
	GraphicsObjectHandle blend_state() const { return _blend_state; }
	GraphicsObjectHandle depth_stencil_state() const { return _depth_stencil_state; }

	void submit();
private:

	bool init();
	bool init_shader(Shader *shader);
	bool compile_shader(Shader *shader);
	bool do_reflection(Shader *shader, void *buf, size_t len, set<string> *used_params);

	RenderKey _key;
	MeshRenderData _render_data;

	string _name;
	Shader *_vertex_shader;
	Shader *_pixel_shader;

	GraphicsObjectHandle _input_layout;

	vector<CBuffer> _constant_buffers;

	int _vert_size;
	vector<float> _vertices;
	int _index_size;
	vector<int> _indices;
	GraphicsObjectHandle _vb;
	GraphicsObjectHandle _ib;

	GraphicsObjectHandle _rasterizer_state;
	GraphicsObjectHandle _sampler_state;  // todo: named samplers
	GraphicsObjectHandle _blend_state;
	GraphicsObjectHandle _depth_stencil_state;

	CD3D11_RASTERIZER_DESC _rasterizer_desc;
	CD3D11_SAMPLER_DESC _sampler_desc;
	CD3D11_BLEND_DESC _blend_desc;
	CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;
};
