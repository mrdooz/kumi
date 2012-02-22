#pragma once

#include "utils.hpp"
#include "property.hpp"
#include "graphics_object_handle.hpp"

struct GraphicsInterface;
struct ResourceInterface;

using std::vector;
using std::string;

struct Material;
class Technique;

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

class Shader {
public:
	enum Type {
		kVertexShader,
		kPixelShader,
		kGeometryShader,
	};

	Shader(const string &entry_point) : _entry_point(entry_point), _valid(false) {}
	virtual ~Shader() {}

	CBufferParam *find_cbuffer_param(const char *name) {
		return find_by_name(name, _cbuffer_params);
	}

	SamplerParam *find_sampler_param(const char *name) {
		return find_by_name(name, _sampler_params);
	}

	ResourceViewParam *find_resource_view_param(const char *name) {
		return find_by_name(name, _resource_view_params);
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
	bool is_valid() const { return _valid; }
  void set_source_filename(const string &filename) { _source_filename = filename; }
  const string &source_filename() const { return _source_filename; }
  void set_entry_point(const string &entry_point) { _entry_point = entry_point; }
  const string &entry_point() const { return _entry_point; }
  void set_obj_filename(const string &filename) { _obj_filename = filename; }
  const string &obj_filename() const { return _obj_filename; }

  vector<CBufferParam> &cbuffer_params() { return _cbuffer_params; }
  vector<SamplerParam> &sampler_params() { return _sampler_params; }
  vector<ResourceViewParam> &resource_view_params() { return _resource_view_params; }

  void set_handle(GraphicsObjectHandle handle) { _handle = handle; }
  GraphicsObjectHandle handle() const { return _handle; }
  void validate() { _valid = true; } // tihi

private:

	bool _valid;
	string _source_filename;
	string _obj_filename;
	string _entry_point;
	vector<CBufferParam> _cbuffer_params;
	vector<SamplerParam> _sampler_params;
	vector<ResourceViewParam> _resource_view_params;
	GraphicsObjectHandle _handle;
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

	//TODO
	//void submit();
	bool init(GraphicsInterface *graphics, ResourceInterface *res);
	bool reload_shaders();
private:

	bool init_shader(GraphicsInterface *graphics, ResourceInterface *res, Shader *shader);
	bool compile_shader(GraphicsInterface *graphics, Shader *shader);
	bool do_reflection(GraphicsInterface *graphics, Shader *shader, void *buf, size_t len, set<string> *used_params);

	//TODO
	//RenderKey _key;
	//MeshRenderData _render_data;

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
	std::vector<std::pair<std::string, GraphicsObjectHandle> > _sampler_states;
	GraphicsObjectHandle _blend_state;
	GraphicsObjectHandle _depth_stencil_state;

	CD3D11_RASTERIZER_DESC _rasterizer_desc;
	std::vector<std::pair<std::string, CD3D11_SAMPLER_DESC>> _sampler_descs;
	CD3D11_BLEND_DESC _blend_desc;
	CD3D11_DEPTH_STENCIL_DESC _depth_stencil_desc;

	vector<Material *> _materials;
};
