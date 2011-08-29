#ifndef _GRAPHICS_HPP_
#define _GRAPHICS_HPP_

#include "utils.hpp"
#include <stack>
#include <type_traits>

struct Io;
struct Shader;
struct Material;
class Technique;

using std::stack;
using std::vector;
using std::pair;

enum FileEvent;

namespace mpl = boost::mpl;

// from http://realtimecollisiondetection.net/blog/?p=86
struct RenderKey {
	enum Cmd {
		kDelete,	// submitted when the object is deleted to avoid using stale data
		kRenderMesh,
		kRenderTechnique,
	};
	union {
		struct {
			uint64_t fullscreen_layer : 2;
			uint64_t depth : 16;
			uint64_t shader : 10;
			uint64_t material : 16;
			uint64_t cmd : 8;
			uint64_t padding : 12;
		};
		uint64_t key;
	};
};

extern RenderKey g_DeleteKey;

static_assert(sizeof(RenderKey) == sizeof(uint64_t), "RenderKey too large");

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
		kInvalid = -1,
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
		kTechnique,
		kVertexShader,
		kPixelShader,
		kShaderResourceView,
	};	
	GraphicsObjectHandle(uint32_t type, uint32_t generation, uint32_t id) : _type(type), _generation(generation), _id(id) {}
	uint32_t _type : 8;
	uint32_t _generation : 8;
	uint32_t _id : 12;
public:
	GraphicsObjectHandle() : _type(kInvalid) {}
	bool is_valid() const { return _type != kInvalid; }
	uint32_t id() const { return _id; }
};

struct MeshRenderData {
	MeshRenderData() : topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}
	GraphicsObjectHandle vb, ib;
	int index_size;
	int index_count;
	int vertex_size;
	int vertex_count;
	D3D_PRIMITIVE_TOPOLOGY topology;
	GraphicsObjectHandle technique;
	uint16 material_id;
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

	const char *vs_profile() const { return _vs_profile.c_str(); }
	const char *ps_profile() const { return _ps_profile.c_str(); }

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

	GraphicsObjectHandle create_constant_buffer(int size);
	GraphicsObjectHandle create_input_layout(const D3D11_INPUT_ELEMENT_DESC *desc, int elems, void *shader_bytecode, int len);
	GraphicsObjectHandle create_static_vertex_buffer(uint32_t data_size, const void* data);
	GraphicsObjectHandle create_static_index_buffer(uint32_t data_size, const void* data);

	GraphicsObjectHandle create_vertex_shader(void *shader_bytecode, int len, const string &id);
	GraphicsObjectHandle create_pixel_shader(void *shader_bytecode, int len, const string &id);

	GraphicsObjectHandle load_technique(const char *filename);
	GraphicsObjectHandle find_technique(const char *name);
	Technique *find_technique2(const char *name);

	GraphicsObjectHandle create_rasterizer_state(const D3D11_RASTERIZER_DESC &desc);
	GraphicsObjectHandle create_blend_state(const D3D11_BLEND_DESC &desc);
	GraphicsObjectHandle create_depth_stencil_state(const D3D11_DEPTH_STENCIL_DESC &desc);
	GraphicsObjectHandle create_sampler_state(const D3D11_SAMPLER_DESC &desc);

	HRESULT create_dynamic_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, ID3D11Buffer** vertex_buffer);
	HRESULT create_static_vertex_buffer(uint32_t vertex_count, uint32_t vertex_size, const void* data, ID3D11Buffer** vertex_buffer);
	HRESULT create_static_index_buffer(uint32_t index_count, uint32_t elem_size, const void* data, ID3D11Buffer** index_buffer);
	void set_vb(ID3D11DeviceContext *context, ID3D11Buffer *buf, uint32_t stride);

	// TODO: this should be on the context
	void submit_command(RenderKey key, void *data);
	void render();

private:
	DISALLOW_COPY_AND_ASSIGN(Graphics);

	Graphics();
	~Graphics();

	void set_cbuffer_params(Technique *technique, Shader *shader, uint16 material_id, uint16 mesh_id);
	void set_samplers(Technique *technique, Shader *shader);
	void set_resource_views(Technique *technique, Shader *shader);

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

	template<class Traits, int N>
	struct IdBufferBase {

		typedef typename Traits::Type T;

		enum { Size = N };
		IdBufferBase() {
			ZeroMemory(&_buffer, sizeof(_buffer));
		}

		int find_free_index() {
			if (!reclaimed_indices.empty()) {
				int tmp = reclaimed_indices.top();
				reclaimed_indices.pop();
				return tmp;
			}
			for (int i = 0; i < N; ++i) {
				if (!Traits::get(_buffer[i]))
					return i;
			}
			return -1;
		}

		typename Traits::Type &operator[](int idx) {
			assert(idx >= 0 && idx < N);
			return _buffer[idx];
		}

		void reclaim_index(int idx) {
			_reclaimed_indices.push(idx);

		}

		typename Traits::Type get(GraphicsObjectHandle handle) {
			return Traits::get(_buffer[handle.id()]);
		}

		typename Traits::Elem _buffer[N];
		stack<int> reclaimed_indices;
	};

	template<class Traits, int N>
	struct SearchableIdBuffer : public IdBufferBase<Traits, N> {

		typename Traits::Elem &operator[](int idx) {
			assert(idx >= 0 && idx < N);
			return _buffer[idx];
		}

		int find_by_token(const typename Traits::Token &t) {
			for (int i = 0; i < Size; ++i) {
				if (Traits::get_token(_buffer[i]) == t)
					return i;
			}
			return -1;
		}
	};

	template<class T, class U>
	struct PairTraits {
		typedef T Type;
		typedef U Token;
		typedef std::pair<T, U> Elem;
		static T& get(Elem &t) {
			return t.first;
		}

		static U get_token(const Elem &e) {
			return e.second;
		}
	};

	template<class T>
	struct SingleTraits {
		typedef T Type;
		typedef T Elem;
		static T& get(Elem& t) {
			return t;
		}
	};

	template<typename T, int N, typename SearchToken = void>
	struct IdBuffer : public
		mpl::if_<typename std::tr1::is_void<SearchToken>::type,
		IdBufferBase<SingleTraits<T>, N>,
		SearchableIdBuffer<PairTraits<T, SearchToken>, N> >::type
	{
	};

	enum { IdCount = 1 << 1 << GraphicsObjectHandle::cIdBits };
	IdBuffer<ID3D11VertexShader *, IdCount, string> _vertex_shaders;
	IdBuffer<ID3D11PixelShader *, IdCount, string> _pixel_shaders;
	IdBuffer<ID3D11Buffer *, IdCount> _vertex_buffers;
	IdBuffer<ID3D11Buffer *, IdCount> _index_buffers;
	IdBuffer<ID3D11Buffer *, IdCount> _constant_buffers;
	IdBuffer<Technique *, IdCount, string> _techniques;
	IdBuffer<ID3D11InputLayout *, IdCount> _input_layouts;

	IdBuffer<ID3D11BlendState *, IdCount> _blend_states;
	IdBuffer<ID3D11DepthStencilState *, IdCount> _depth_stencil_states;
	IdBuffer<ID3D11RasterizerState *, IdCount> _rasterizer_states;
	IdBuffer<ID3D11SamplerState *, IdCount> _sampler_states;
	IdBuffer<ID3D11ShaderResourceView *, IdCount> _shader_resource_views;

	typedef pair<RenderKey, void *> RenderCmd;
	vector<RenderCmd > _render_commands;

	string _vs_profile;
	string _ps_profile;
};

#define GRAPHICS Graphics::instance()

#define D3D_DEVICE Graphics::instance().device()
#define D3D_CONTEXT Graphics::instance().context()

#endif
