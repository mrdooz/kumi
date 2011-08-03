#ifndef _GRAPHICS_HPP_
#define _GRAPHICS_HPP_

#include "utils.hpp"


// from http://realtimecollisiondetection.net/blog/?p=86
struct RenderKey {
	enum Cmd {
		kRenderMesh,
	};
	union {
		struct {
			uint64_t fullscreen_layer : 2;
			uint64_t depth : 30;
			uint64_t cmd : 32;
		};
		uint64_t key;
	};
};

struct RenderTargetData {
	void reset() {
		texture = NULL;
		depth_buffer = NULL;
		rtv = NULL;
		dsv = NULL;
		srv = NULL;
	}
	D3D11_TEXTURE2D_DESC texture_desc;
	D3D11_TEXTURE2D_DESC depth_buffer_desc;
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	CComPtr<ID3D11Texture2D> texture;
	CComPtr<ID3D11Texture2D> depth_buffer;
	CComPtr<ID3D11RenderTargetView> rtv;
	CComPtr<ID3D11DepthStencilView> dsv;
	CComPtr<ID3D11ShaderResourceView> srv;
};

struct TextureData {
	void reset() {
		texture.Release();
		srv.Release();
	}
	D3D11_TEXTURE2D_DESC texture_desc;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	CComPtr<ID3D11Texture2D> texture;
	CComPtr<ID3D11ShaderResourceView> srv;
};

class GraphicsObjectHandle {
	friend class Graphics;
	enum { 
		cTypeBits = 8,
		cGenerationBits = 8,
		cIdBits = 12 
	};
	enum Type {
		kInvalid,
		kContext,
		kVertexBuffer,
		kIndexBuffer,
		kConstantBuffer,
		kTexture,
		kShader,
		kInputLayout,
		kBlendState,
		kRasterizerState,
		kSamplerState,
		kDepthStencilState,
	};	
	GraphicsObjectHandle(uint32_t type, uint32_t generation, uint32_t id) : type(type), generation(generation), id(id) {}
	uint32_t type : 8;
	uint32_t generation : 8;
	uint32_t id : 12;
public:
	GraphicsObjectHandle() : type(kInvalid) {}
	bool is_valid() const { return type != kInvalid; }
};

struct MeshRenderData {
	GraphicsObjectHandle vb, ib;
};

enum RenderCommand {

};

class Context {
	friend class Graphics;
public:

private:
	CComPtr<ID3D11DeviceContext> _context;
};

class Graphics {
public:

	static Graphics& instance();

	bool	init(const HWND hwnd, const int width, const int height);
	bool	close();
  void	clear(const XMFLOAT4& c);
  void	clear();
  void	set_clear_color(const XMFLOAT4& c) { _clear_color = c; }
	void	present();
	void	resize(const int width, const int height);

	ID3D11Device* device() { return _device; }
  ID3D11DeviceContext* context() { return _immediate_context._context; }

  const D3D11_VIEWPORT& viewport() const { return _viewport; }

	void set_default_render_target();

  D3D_FEATURE_LEVEL feature_level() const { return _feature_level; }

	bool create_render_target(int width, int height, RenderTargetData *out);
	bool create_texture(const D3D11_TEXTURE2D_DESC &desc, TextureData *out);

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

	GraphicsObjectHandle create_static_vertex_buffer(uint32_t data_size, const void* data);
	GraphicsObjectHandle create_static_index_buffer(uint32_t data_size, const void* data);

	HRESULT create_dynamic_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, ID3D11Buffer** vertex_buffer);
	HRESULT create_static_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, const void* data, ID3D11Buffer** vertex_buffer);
	HRESULT create_static_index_buffer(uint32_t index_count, uint32_t elem_size, const void* data, ID3D11Buffer** index_buffer);
	void set_vb(ID3D11DeviceContext *context, ID3D11Buffer *buf, uint32_t stride);

	void submit_command(RenderKey key, void *data);
	void render();

private:
	DISALLOW_COPY_AND_ASSIGN(Graphics);

	Graphics();
	~Graphics();

  bool create_back_buffers(int width, int height);
	static Graphics* _instance;

	int _width;
	int _height;
	D3D11_VIEWPORT _viewport;
	DXGI_FORMAT _buffer_format;
  D3D_FEATURE_LEVEL _feature_level;
	CComPtr<ID3D11Device> _device;
	CComPtr<IDXGISwapChain> _swap_chain;
	CComPtr<ID3D11RenderTargetView> _render_target_view;
	CComPtr<ID3D11Texture2D> _depth_stencil;
	CComPtr<ID3D11DepthStencilView> _depth_stencil_view;

	CComPtr<ID3D11Texture2D> _back_buffer;
	CComPtr<ID3D11ShaderResourceView> _shared_texture_view;
	CComPtr<ID3D11Texture2D> _shared_texture;
	CComPtr<IDXGIKeyedMutex> _keyed_mutex_10;
	CComPtr<IDXGIKeyedMutex> _keyed_mutex_11;

  CComPtr<ID3D11Debug> _d3d_debug;

  CComPtr<ID3D11RasterizerState> _default_rasterizer_state;
  CComPtr<ID3D11DepthStencilState> _default_depth_stencil_state;
  CComPtr<ID3D11SamplerState> _default_sampler_state;
  float _default_blend_factors[4];
  CComPtr<ID3D11BlendState> _default_blend_state;

	XMFLOAT4 _clear_color;
  DWORD _start_fps_time;
  int32_t _frame_count;
  float _fps;
	Context _immediate_context;

	ID3D11Buffer *_vertex_buffers[1 << GraphicsObjectHandle::cIdBits];
	ID3D11Buffer *_index_buffers[1 << GraphicsObjectHandle::cIdBits];

	typedef std::pair<RenderKey, void *> RenderCmd;
	std::vector<RenderCmd > _render_commands;
};

#define GRAPHICS Graphics::instance()

#define D3D_DEVICE Graphics::instance().device()
#define D3D_CONTEXT Graphics::instance().context()

#endif
