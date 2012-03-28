#ifndef _GRAPHICS_HPP_
#define _GRAPHICS_HPP_

#include "utils.hpp"
#include "id_buffer.hpp"
#include "graphics_object_handle.hpp"
#include "graphics_interface.hpp"
#include "technique.hpp"

struct Io;
class Shader;
struct Material;

using std::map;
using std::string;
using std::vector;
using std::pair;

enum FileEvent;

struct RenderTargetData;
struct TextureData;

struct RenderTargetData {
	RenderTargetData() {
		reset();
	}

	void reset() {
		texture = NULL;
		depth_stencil = NULL;
		rtv = NULL;
		dsv = NULL;
		srv = NULL;
	}

	operator bool() { return texture || depth_stencil || rtv || dsv || srv; }

	D3D11_TEXTURE2D_DESC texture_desc;
	D3D11_TEXTURE2D_DESC depth_buffer_desc;
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	CComPtr<ID3D11Texture2D> texture;
	CComPtr<ID3D11Texture2D> depth_stencil;
	CComPtr<ID3D11RenderTargetView> rtv;
	CComPtr<ID3D11DepthStencilView> dsv;
	CComPtr<ID3D11ShaderResourceView> srv;
};

struct TextureData {
	~TextureData() {
		reset();
	}

	void reset() {
		texture.Release();
		srv.Release();
	}
	operator bool() { return texture || srv; }
	D3D11_TEXTURE2D_DESC texture_desc;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	CComPtr<ID3D11Texture2D> texture;
	CComPtr<ID3D11ShaderResourceView> srv;
};


class Graphics : public GraphicsInterface {
public:

	struct BackedResources {
		BackedResources()
			: _vertex_shaders(release_obj<ID3D11VertexShader *>)
			, _pixel_shaders(release_obj<ID3D11PixelShader *>)
			, _vertex_buffers(release_obj<ID3D11Buffer *>)
			, _index_buffers(release_obj<ID3D11Buffer *>)
			, _constant_buffers(release_obj<ID3D11Buffer *>)
			, _techniques(delete_obj<Technique *>)
			, _input_layouts(release_obj<ID3D11InputLayout *>)
			, _blend_states(release_obj<ID3D11BlendState *>)
			, _depth_stencil_states(release_obj<ID3D11DepthStencilState *>)
			, _rasterizer_states(release_obj<ID3D11RasterizerState *>)
			, _sampler_states(release_obj<ID3D11SamplerState *>)
			, _shader_resource_views(release_obj<ID3D11ShaderResourceView *>)
			, _textures(delete_obj<TextureData *>)
			, _render_targets(delete_obj<RenderTargetData *>)
		{
		}

		enum { IdCount = 1 << 1 << GraphicsObjectHandle::cIdBits };
		SearchableIdBuffer<ID3D11VertexShader *, string, IdCount> _vertex_shaders;
		SearchableIdBuffer<ID3D11PixelShader *, string, IdCount> _pixel_shaders;
		IdBuffer<ID3D11Buffer *, IdCount> _vertex_buffers;
		IdBuffer<ID3D11Buffer *, IdCount> _index_buffers;
		IdBuffer<ID3D11Buffer *, IdCount> _constant_buffers;
		SearchableIdBuffer<Technique *, string, IdCount> _techniques;
		IdBuffer<ID3D11InputLayout *, IdCount> _input_layouts;

		IdBuffer<ID3D11BlendState *, IdCount> _blend_states;
		IdBuffer<ID3D11DepthStencilState *, IdCount> _depth_stencil_states;
		IdBuffer<ID3D11RasterizerState *, IdCount> _rasterizer_states;
		IdBuffer<ID3D11SamplerState *, IdCount> _sampler_states;
		IdBuffer<ID3D11ShaderResourceView *, IdCount> _shader_resource_views;
		SearchableIdBuffer<TextureData *, string, IdCount> _textures;
		SearchableIdBuffer<RenderTargetData *, string, IdCount> _render_targets;
	};

	static bool create(ResourceInterface *ri);
	static Graphics& instance();
  static void close();

	bool	init(const HWND hwnd, const int width, const int height);
	void	clear(const XMFLOAT4& c);
	void	clear(GraphicsObjectHandle h, const XMFLOAT4 &c);
	void	present();
	void	resize(const int width, const int height);

	const char *vs_profile() const { return _vs_profile.c_str(); }
	const char *ps_profile() const { return _ps_profile.c_str(); }

	ID3D11Device* device() { return _device; }
  ID3D11DeviceContext* context() { return _immediate_context; }

  const D3D11_VIEWPORT& viewport() const { return _viewport; }

	void set_default_render_target();

  D3D_FEATURE_LEVEL feature_level() const { return _feature_level; }

	BackedResources *get_backed_resources() { return &_res; }

	GraphicsObjectHandle create_render_target(int width, int height, bool shader_resource, const char *name);
	GraphicsObjectHandle create_texture(const D3D11_TEXTURE2D_DESC &desc, const char *name);

	bool map(GraphicsObjectHandle h, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *res);
	void unmap(GraphicsObjectHandle h, UINT sub);
	void copy_resource(GraphicsObjectHandle dst, GraphicsObjectHandle src);

	// Create a texture, and fill it with data
	bool create_texture(int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, TextureData *out);

  ID3D11RasterizerState *default_rasterizer_state() const { return _default_rasterizer_state; }
  ID3D11DepthStencilState *default_depth_stencil_state() const { return _default_depth_stencil_state; }
  uint32_t default_stencil_ref() const { return 0; }
  ID3D11BlendState *default_blend_state() const { return _default_blend_state; }
  const float *default_blend_factors() const { return _default_blend_factors; }
  uint32_t default_sample_mask() const { return 0xffffffff; }

  float fps() const { return _fps; }
  int width() const { return _width; }
  int height() const { return _height; }
	void size(int *width, int *height) const { *width = _width; *height = _height; }

	GraphicsObjectHandle create_constant_buffer(int size);
	GraphicsObjectHandle create_input_layout(const D3D11_INPUT_ELEMENT_DESC *desc, int elems, void *shader_bytecode, int len);
	GraphicsObjectHandle create_static_vertex_buffer(uint32_t data_size, const void* data);
	GraphicsObjectHandle create_static_index_buffer(uint32_t data_size, const void* data);

	GraphicsObjectHandle create_vertex_shader(void *shader_bytecode, int len, const string &id);
	GraphicsObjectHandle create_pixel_shader(void *shader_bytecode, int len, const string &id);

	bool load_techniques(const char *filename, bool add_materials);
	GraphicsObjectHandle find_technique(const char *name);

	GraphicsObjectHandle create_rasterizer_state(const D3D11_RASTERIZER_DESC &desc);
	GraphicsObjectHandle create_blend_state(const D3D11_BLEND_DESC &desc);
	GraphicsObjectHandle create_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC &desc);
	GraphicsObjectHandle create_sampler_state(const D3D11_SAMPLER_DESC &desc);

	HRESULT create_dynamic_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, ID3D11Buffer** vertex_buffer);
	HRESULT create_static_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, const void* data, ID3D11Buffer** vertex_buffer);
	HRESULT create_static_index_buffer(uint32_t index_count, uint32_t elem_size, const void* data, ID3D11Buffer** index_buffer);
	void set_vb(ID3D11DeviceContext *context, ID3D11Buffer *buf, uint32_t stride);

	GraphicsObjectHandle default_rt_handle() const { return _default_rt_handle; }

	void set_cbuffer_params(Technique *technique, Shader *shader, uint16 material_id, intptr_t mesh_id);
	void set_samplers(Technique *technique, Shader *shader);
	void set_resource_views(Technique *technique, Shader *shader, int *resources_set);
	void unbind_resource_views(int resource_bitmask);

private:
	DISALLOW_COPY_AND_ASSIGN(Graphics);

	Graphics(ResourceInterface *ri);
	~Graphics();

	bool create_render_target(int width, int height, bool shader_resource, RenderTargetData *out);
	bool create_texture(const D3D11_TEXTURE2D_DESC &desc, TextureData *out);

  bool create_back_buffers(int width, int height);

  void technique_file_changed(const char *filename, void *token);
  void shader_file_changed(const char *filename, void *token);

	static Graphics* _instance;

	int _width;
	int _height;
	D3D11_VIEWPORT _viewport;
	DXGI_FORMAT _buffer_format;
  D3D_FEATURE_LEVEL _feature_level;
	CComPtr<ID3D11Device> _device;
	CComPtr<IDXGISwapChain> _swap_chain;

	GraphicsObjectHandle _default_rt_handle;

	CComPtr<ID3D11Debug> _d3d_debug;

	CComPtr<ID3D11RasterizerState> _default_rasterizer_state;
	CComPtr<ID3D11DepthStencilState> _default_depth_stencil_state;
	CComPtr<ID3D11SamplerState> _default_sampler_state;
	float _default_blend_factors[4];
	CComPtr<ID3D11BlendState> _default_blend_state;

	DWORD _start_fps_time;
	int32_t _frame_count;
	float _fps;
	CComPtr<ID3D11DeviceContext> _immediate_context;

	ResourceInterface *_ri;
	BackedResources _res;

	std::map<string, vector<string> > _techniques_by_file;

	string _vs_profile;
	string _ps_profile;

};

#define GRAPHICS Graphics::instance()

#define D3D_DEVICE Graphics::instance().device()
#define D3D_CONTEXT Graphics::instance().context()

#endif
