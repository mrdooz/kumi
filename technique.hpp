#pragma once

#include "utils.hpp"
#include "property_manager.hpp"
#include "graphics.hpp"
#include "graphics_submit.hpp"

using std::vector;
using std::string;

namespace PropertySource {
	enum Enum {
		kUnknown,
		kMaterial,
		kSystem,
		kUser,
		kMesh,
		kTechnique,
	};
}

struct ParamBase {
	ParamBase(const string &name, PropertyType::Enum type, PropertySource::Enum source) : name(name), type(type), source(source) {}
	string name;
	PropertyType::Enum type;
	PropertySource::Enum source;
};

struct CBufferParam : public ParamBase {
	CBufferParam(const string &name, PropertyType::Enum type, PropertySource::Enum source) : ParamBase(name, type, source) {}

	union DefaultValue {
		int _int;
		float _float[16];
	};

	int cbuffer;
	int start_offset;  // offset in cbuffer
	int size;
	DefaultValue default_value;
};

struct SamplerParam : public ParamBase {
	SamplerParam(const string &name, PropertyType::Enum type, PropertySource::Enum source) : ParamBase(name, type, source) {}
	int bind_point;
};

struct ResourceViewParam : public ParamBase {
	ResourceViewParam(const string &name, PropertyType::Enum type, PropertySource::Enum source) : ParamBase(name, type, source) {}
	int bind_point;
};

struct Shader {
	enum Type {
		kVertexShader,
		kPixelShader,
		kGeometryShader,
	};

	Shader(const string &entry_point) : entry_point(entry_point), valid(false) {}

	CBufferParam *find_cbuffer_param(const char *name) {
		return find_by_name(name, cbuffer_params);
	}

	SamplerParam *find_sampler_param(const char *name) {
		return find_by_name(name, sampler_params);
	}

	ResourceViewParam *find_resource_view_param(const char *name) {
		return find_by_name(name, resource_view_params);
	}

	template<class T>
	T* find_by_name(const char *name, vector<T> &v) {
		for (size_t i = 0; i < v.size(); ++i) {
			if (v[i].name == name)
				return &v[i];
		}
		return NULL;
	}

	virtual void set_buffers() = 0;
	virtual Type type() const = 0;
	bool is_valid() const { return valid; }

	bool valid;
	string source_filename;
	string obj_filename;
	string entry_point;
	vector<CBufferParam> cbuffer_params;
	vector<SamplerParam> sampler_params;
	vector<ResourceViewParam> resource_view_params;
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

	const string &name() const { return _name; }

	GraphicsObjectHandle input_layout() const { return _input_layout; }
	Shader *vertex_shader() { return _vertex_shader; }
	Shader *pixel_shader() { return _pixel_shader; }

	vector<CBuffer> &get_cbuffers() { return _constant_buffers; }

	GraphicsObjectHandle rasterizer_state() const { return _rasterizer_state; }
	GraphicsObjectHandle blend_state() const { return _blend_state; }
	GraphicsObjectHandle depth_stencil_state() const { return _depth_stencil_state; }
	GraphicsObjectHandle sampler_state(const char *name) const;

	GraphicsObjectHandle vb() const { return _vb; }
	GraphicsObjectHandle ib() const { return _ib; }
	int vertex_size() const { return _vertex_size; }
	DXGI_FORMAT index_format() const { return _index_format; }
	int index_count() const { return (int)_indices.size(); }

	void submit();
	bool init();
private:

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

	int _vertex_size;
	vector<float> _vertices;
	DXGI_FORMAT _index_format;
	vector<int> _indices;
	GraphicsObjectHandle _vb;
	GraphicsObjectHandle _ib;

	GraphicsObjectHandle _rasterizer_state;
	vector<pair<string, GraphicsObjectHandle> > _sampler_states;
	GraphicsObjectHandle _blend_state;
	GraphicsObjectHandle _depth_stencil_state;

	CD3D11_RASTERIZER_DESC _rasterizer_desc;
	vector<pair<string, CD3D11_SAMPLER_DESC>> _sampler_descs;
	CD3D11_BLEND_DESC _blend_desc;
	CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;
};
