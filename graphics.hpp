#ifndef _GRAPHICS_HPP_
#define _GRAPHICS_HPP_

#include "utils.hpp"
#include "id_buffer.hpp"
#include "graphics_object_handle.hpp"
#include "technique.hpp"
#include "property_manager.hpp"
#include "tracked_location.hpp"

struct Io;
class Shader;
class Material;

using std::map;
using std::string;
using std::vector;
using std::pair;

enum FileEvent;

#if WITH_GWEN
struct IFW1Factory;
struct IFW1FontWrapper;
#endif

class Graphics {
public:

  template<class Resource, class Desc>
  struct ResourceAndDesc {
    void release() {
      resource.Release();
    }
    CComPtr<Resource> resource;
    Desc desc;
  };

  struct RenderTargetResource {
    RenderTargetResource() {
      reset();
    }

    void reset() {
      texture.release();
      depth_stencil.release();
      rtv.release();
      dsv.release();
      srv.release();
    }

    ResourceAndDesc<ID3D11Texture2D, D3D11_TEXTURE2D_DESC> texture;
    ResourceAndDesc<ID3D11Texture2D, D3D11_TEXTURE2D_DESC> depth_stencil;
    ResourceAndDesc<ID3D11RenderTargetView, D3D11_RENDER_TARGET_VIEW_DESC> rtv;
    ResourceAndDesc<ID3D11DepthStencilView, D3D11_DEPTH_STENCIL_VIEW_DESC> dsv;
    ResourceAndDesc<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC> srv;
  };

  struct TextureResource {
    void reset() {
      texture.release();
      view.release();
    }
    ResourceAndDesc<ID3D11Texture2D, D3D11_TEXTURE2D_DESC> texture;
    ResourceAndDesc<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC> view;
  };

  struct SimpleResource {
    void reset() {
      resource.Release();
      view.release();
    }
    CComPtr<ID3D11Resource> resource;
    ResourceAndDesc<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC> view;
  };
#if 0
  struct BackedResources {
    ~BackedResources() {

    }
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
      , _textures(delete_obj<TextureResource *>)
      , _render_targets(delete_obj<RenderTargetResource *>)
      , _resources(delete_obj<SimpleResource *>)
#if WITH_GWEN
      , _font_wrappers(release_obj<IFW1FontWrapper *>)
#endif
    {
    }

    enum { IdCount = 1 << GraphicsObjectHandle::cIdBits };
    SearchableIdBuffer<string, ID3D11VertexShader *, IdCount> _vertex_shaders;
    SearchableIdBuffer<string, ID3D11PixelShader *, IdCount> _pixel_shaders;
    IdBuffer<ID3D11Buffer *, IdCount> _vertex_buffers;
    IdBuffer<ID3D11Buffer *, IdCount> _index_buffers;
    IdBuffer<ID3D11Buffer *, IdCount> _constant_buffers;
    SearchableIdBuffer<string, Technique *, IdCount> _techniques;
    IdBuffer<ID3D11InputLayout *, IdCount> _input_layouts;

    IdBuffer<ID3D11BlendState *, IdCount> _blend_states;
    IdBuffer<ID3D11DepthStencilState *, IdCount> _depth_stencil_states;
    IdBuffer<ID3D11RasterizerState *, IdCount> _rasterizer_states;
    IdBuffer<ID3D11ShaderResourceView *, IdCount> _shader_resource_views;
    SearchableIdBuffer<string, ID3D11SamplerState *, IdCount> _sampler_states;
    SearchableIdBuffer<string, TextureResource *, IdCount> _textures;
    SearchableIdBuffer<string, RenderTargetResource *, IdCount> _render_targets;
    SearchableIdBuffer<string, SimpleResource *, IdCount> _resources;
#if WITH_GWEN
    SearchableIdBuffer<std::wstring,  IFW1FontWrapper *, IdCount> _font_wrappers;
#endif
  };
#endif
  static bool create();
  inline static Graphics& instance() {
    assert(_instance);
    return *_instance;
  }

  static bool close();

  bool	init(const HWND hwnd, const int width, const int height);
  void	clear(const XMFLOAT4& c);
  void	clear(GraphicsObjectHandle h, const XMFLOAT4 &c);
  void	present();
  void	resize(const int width, const int height);

  const char *vs_profile() const { return _vs_profile.c_str(); }
  const char *ps_profile() const { return _ps_profile.c_str(); }

  GraphicsObjectHandle create_constant_buffer(const TrackedLocation &loc, int size, bool dynamic);
  GraphicsObjectHandle create_input_layout(const TrackedLocation &loc, const D3D11_INPUT_ELEMENT_DESC *desc, int elems, const void *shader_bytecode, int len);
  GraphicsObjectHandle create_static_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data);
  GraphicsObjectHandle create_static_index_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data);
  GraphicsObjectHandle create_dynamic_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size);

  GraphicsObjectHandle create_vertex_shader(const TrackedLocation &loc, void *shader_bytecode, int len, const string &id);
  GraphicsObjectHandle create_pixel_shader(const TrackedLocation &loc, void *shader_bytecode, int len, const string &id);

  GraphicsObjectHandle create_rasterizer_state(const TrackedLocation &loc, const D3D11_RASTERIZER_DESC &desc);
  GraphicsObjectHandle create_blend_state(const TrackedLocation &loc, const D3D11_BLEND_DESC &desc);
  GraphicsObjectHandle create_depth_stencil_state(const TrackedLocation &loc, const D3D11_DEPTH_STENCIL_DESC &desc);
  GraphicsObjectHandle create_sampler_state(const TrackedLocation &loc, const D3D11_SAMPLER_DESC &desc);

#if WITH_GWEN
  GraphicsObjectHandle get_or_create_font_family(const std::wstring &name);
  bool measure_text(GraphicsObjectHandle font, const std::wstring &family, const std::wstring &text, float size, uint32 flags, FW1_RECTF *rect);
#endif

  ID3D11Device* device() { return _device; }
  ID3D11DeviceContext* context() { return _immediate_context; }

  const D3D11_VIEWPORT& viewport() const { return _viewport; }

  void set_default_render_target();

  D3D_FEATURE_LEVEL feature_level() const { return _feature_level; }

  //BackedResources *get_backed_resources() { return &_res; }

  GraphicsObjectHandle create_render_target(const TrackedLocation &loc, int width, int height, bool shader_resource, const char *name);
  GraphicsObjectHandle create_render_target(const TrackedLocation &loc, int width, int height, bool shader_resource, bool depth_buffer, DXGI_FORMAT format, const char *name);
  GraphicsObjectHandle create_texture(const TrackedLocation &loc, const D3D11_TEXTURE2D_DESC &desc, const char *name);
  GraphicsObjectHandle load_texture(const char *filename, const char *friendly_name, D3DX11_IMAGE_INFO *info);
  GraphicsObjectHandle get_texture(const char *filename);
  bool read_texture(const char *filename, D3DX11_IMAGE_INFO *info, uint32 *pitch, vector<uint8> *bits);

  bool map(GraphicsObjectHandle h, UINT sub, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE *res);
  void unmap(GraphicsObjectHandle h, UINT sub);
  void copy_resource(GraphicsObjectHandle dst, GraphicsObjectHandle src);

  // Create a texture, and fill it with data
  bool create_texture(const TrackedLocation &loc, int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, TextureResource *out);
  GraphicsObjectHandle create_texture(const TrackedLocation &loc, int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, const char *friendly_name);

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

  bool load_techniques(const char *filename, bool add_materials);
  Technique *get_technique(GraphicsObjectHandle h);
  GraphicsObjectHandle find_technique(const char *name);
  void get_technique_states(const char *technique, GraphicsObjectHandle *rs, GraphicsObjectHandle *bs, GraphicsObjectHandle *dss);
  GraphicsObjectHandle find_input_layout(const std::string &technique_name);
  GraphicsObjectHandle find_resource(const std::string &name);
  GraphicsObjectHandle find_sampler_state(const std::string &name);

  HRESULT create_dynamic_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, ID3D11Buffer** vertex_buffer);
  HRESULT create_static_vertex_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data, ID3D11Buffer** vertex_buffer);
  HRESULT create_static_index_buffer(const TrackedLocation &loc, uint32_t buffer_size, const void* data, ID3D11Buffer** index_buffer);
  void set_vb(ID3D11Buffer *buf, uint32_t stride);

  GraphicsObjectHandle default_render_target() const { return _default_render_target; }

  //void set_samplers(Technique *technique, Shader *shader);
  void set_resource_views(Technique *technique, Shader *shader, int *resources_set);
  void unbind_resource_views(int resource_bitmask);

  ID3D11ClassLinkage *get_class_linkage();

  void add_shader_flag(const std::string &flag);
  int get_shader_flag(const std::string &flag);

  // Rendering related
  template <typename T>
  T *alloc_command_data() {
    void *t = raw_alloc(sizeof(T));
    T *tt = new (t)T();
    return tt;
  }
  void *raw_alloc(size_t size);
  void submit_command(const TrackedLocation &location, RenderKey key, void *data);
  void submit_technique(GraphicsObjectHandle technique);
  void render();


private:
  DISALLOW_COPY_AND_ASSIGN(Graphics);

  Graphics();
  ~Graphics();

  GraphicsObjectHandle make_goh(GraphicsObjectHandle::Type type, int idx);

  bool create_render_target(const TrackedLocation &loc, int width, int height, bool shader_resource, bool depth_buffer, DXGI_FORMAT format, RenderTargetResource *out);
  bool create_texture(const TrackedLocation &loc, const D3D11_TEXTURE2D_DESC &desc, TextureResource *out);

  bool create_back_buffers(int width, int height);

  bool technique_file_changed(const char *filename, void *token);
  bool shader_file_changed(const char *filename, void *token);

  // given texture data and a name, insert it into the GOH chain
  GraphicsObjectHandle insert_texture(TextureResource *data, const char *friendly_name);

  void validate_command(RenderKey key, const void *data);

  // Renderer related
  void set_vs(GraphicsObjectHandle vs);
  void set_ps(GraphicsObjectHandle ps);
  void set_layout(GraphicsObjectHandle layout);
  void set_vb(GraphicsObjectHandle vb, int vertex_size);
  void set_ib(GraphicsObjectHandle ib, DXGI_FORMAT format);
  void set_topology(D3D11_PRIMITIVE_TOPOLOGY top);
  void set_rs(GraphicsObjectHandle rs);
  void set_dss(GraphicsObjectHandle dss, UINT stencil_ref);
  void set_bs(GraphicsObjectHandle bs, const float *blend_factors, UINT sample_mask);
  void set_samplers(const GraphicsObjectHandle *sampler_handles, int first_sampler, int num_samplers);
  void set_shader_resources(const GraphicsObjectHandle *view_handles, int first_view, int num_views);
  void unset_shader_resource(int first_view, int num_views);
  void set_cbuffer(const vector<CBuffer> &vs, const vector<CBuffer> &ps);
  void draw_indexed(int count, int start_index, int base_vertex);

  GraphicsObjectHandle prev_vs, prev_ps, prev_layout;
  GraphicsObjectHandle prev_rs, prev_bs, prev_dss;
  GraphicsObjectHandle prev_ib, prev_vb;
  GraphicsObjectHandle prev_samplers[MAX_SAMPLERS];
  GraphicsObjectHandle prev_views[MAX_SAMPLERS];
  D3D11_PRIMITIVE_TOPOLOGY prev_topology;


  struct RenderCmd {
    RenderCmd(const TrackedLocation &location, RenderKey key, void *data) : location(location), key(key), data(data) {}
    TrackedLocation location;
    RenderKey key;
    void *data;
  };

  std::vector<RenderCmd > _render_commands;

  std::vector<uint8> _effect_data;
  int _effect_data_ofs;

  // resources
  enum { IdCount = 1 << GraphicsObjectHandle::cIdBits };
  SearchableIdBuffer<string, ID3D11VertexShader *, IdCount> _vertex_shaders;
  SearchableIdBuffer<string, ID3D11PixelShader *, IdCount> _pixel_shaders;
  IdBuffer<ID3D11Buffer *, IdCount> _vertex_buffers;
  IdBuffer<ID3D11Buffer *, IdCount> _index_buffers;
  IdBuffer<ID3D11Buffer *, IdCount> _constant_buffers;
  SearchableIdBuffer<string, Technique *, IdCount> _techniques;
  IdBuffer<ID3D11InputLayout *, IdCount> _input_layouts;

  IdBuffer<ID3D11BlendState *, IdCount> _blend_states;
  IdBuffer<ID3D11DepthStencilState *, IdCount> _depth_stencil_states;
  IdBuffer<ID3D11RasterizerState *, IdCount> _rasterizer_states;
  IdBuffer<ID3D11ShaderResourceView *, IdCount> _shader_resource_views;
  SearchableIdBuffer<string, ID3D11SamplerState *, IdCount> _sampler_states;
  SearchableIdBuffer<string, TextureResource *, IdCount> _textures;
  SearchableIdBuffer<string, RenderTargetResource *, IdCount> _render_targets;
  SearchableIdBuffer<string, SimpleResource *, IdCount> _resources;
#if WITH_GWEN
  SearchableIdBuffer<std::wstring,  IFW1FontWrapper *, IdCount> _font_wrappers;
#endif

  static Graphics* _instance;

  int _width;
  int _height;
  D3D11_VIEWPORT _viewport;
  DXGI_FORMAT _buffer_format;
  D3D_FEATURE_LEVEL _feature_level;
  CComPtr<ID3D11Device> _device;
  CComPtr<IDXGISwapChain> _swap_chain;

  GraphicsObjectHandle _default_render_target;
  GraphicsObjectHandle _dummy_texture;

#ifdef _DEBUG
  CComPtr<ID3D11Debug> _d3d_debug;
#endif

  CComPtr<ID3D11RasterizerState> _default_rasterizer_state;
  CComPtr<ID3D11DepthStencilState> _default_depth_stencil_state;
  CComPtr<ID3D11SamplerState> _default_sampler_state;
  float _default_blend_factors[4];
  CComPtr<ID3D11BlendState> _default_blend_state;

  DWORD _start_fps_time;
  int32_t _frame_count;
  float _fps;
  CComPtr<ID3D11DeviceContext> _immediate_context;

  CComPtr<ID3D11ClassLinkage> _class_linkage;

  std::map<string, vector<string> > _techniques_by_file;

  string _vs_profile;
  string _ps_profile;

#if WITH_GWEN
  CComPtr<IFW1Factory> _fw1_factory;
#endif

  PropertyId _screen_size_id;
  std::map<std::string, int> _shader_flags;
};

#define GRAPHICS Graphics::instance()

#define D3D_DEVICE Graphics::instance().device()
#define D3D_CONTEXT Graphics::instance().context()

#endif
