#ifndef _GRAPHICS_HPP_
#define _GRAPHICS_HPP_

#include "utils.hpp"
#include "id_buffer.hpp"
#include "graphics_object_handle.hpp"
#include "technique.hpp"
#include "tracked_location.hpp"

struct Io;
class Shader;
class Material;

enum FileEvent;

class Graphics {
  friend class DeferredContext;
  friend class PackedResourceManager;
  friend class ResourceManager;
public:

  enum BufferFlags {
    kCreateMipMaps        = 1 << 0,
    kCreateDepthBuffer    = 1 << 1,
    kCreateSrv            = 1 << 2,
    kCreateUav            = 1 << 3,
  };

  enum PredefinedGeometry {
    kGeomFsQuadPos,
    kGeomFsQuadPosTex,
  };

  template<class Resource, class Desc>
  struct ResourceAndDesc {
    void release() {
      resource.Release();
    }
    CComPtr<Resource> resource;
    Desc desc;
  };

  struct RenderTargetResource {
    RenderTargetResource() : in_use(true) {
      reset();
    }

    void reset() {
      texture.release();
      depth_stencil.release();
      rtv.release();
      dsv.release();
      srv.release();
      uav.release();
    }

    bool in_use;
    ResourceAndDesc<ID3D11Texture2D, D3D11_TEXTURE2D_DESC> texture;
    ResourceAndDesc<ID3D11Texture2D, D3D11_TEXTURE2D_DESC> depth_stencil;
    ResourceAndDesc<ID3D11RenderTargetView, D3D11_RENDER_TARGET_VIEW_DESC> rtv;
    ResourceAndDesc<ID3D11DepthStencilView, D3D11_DEPTH_STENCIL_VIEW_DESC> dsv;
    ResourceAndDesc<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC> srv;
    ResourceAndDesc<ID3D11UnorderedAccessView, D3D11_UNORDERED_ACCESS_VIEW_DESC> uav;
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

  struct StructuredBuffer {
    ResourceAndDesc<ID3D11Buffer, D3D11_BUFFER_DESC> buffer;
    ResourceAndDesc<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC> srv;
    ResourceAndDesc<ID3D11UnorderedAccessView, D3D11_UNORDERED_ACCESS_VIEW_DESC> uav;
  };

  static bool create();
  inline static Graphics& instance() {
    KASSERT(_instance);
    return *_instance;
  }

  static bool close();

  bool config(HINSTANCE hInstance);
  bool	init(WNDPROC wndProc);

  void	present();
  //void	resize(int width, int height);

  const char *vs_profile() const { return _vs_profile; }
  const char *ps_profile() const { return _ps_profile; }
  const char *cs_profile() const { return _cs_profile; }
  const char *gs_profile() const { return _gs_profile; }

  void get_predefined_geometry(PredefinedGeometry geom, GraphicsObjectHandle *vb, int *vertex_size, GraphicsObjectHandle *ib, DXGI_FORMAT *index_format, int *index_count);

  GraphicsObjectHandle create_input_layout(const TrackedLocation &loc, const std::vector<D3D11_INPUT_ELEMENT_DESC> &desc, const std::vector<char> &shader_bytecode);

  GraphicsObjectHandle create_buffer(const TrackedLocation &loc, D3D11_BIND_FLAG bind, int size, bool dynamic, const void* data);

  GraphicsObjectHandle create_vertex_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const std::string &id);
  GraphicsObjectHandle create_pixel_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const std::string &id);
  GraphicsObjectHandle create_compute_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const std::string &id);
  GraphicsObjectHandle create_geometry_shader(const TrackedLocation &loc, const std::vector<char> &shader_bytecode, const std::string &id);

  GraphicsObjectHandle create_rasterizer_state(const TrackedLocation &loc, const D3D11_RASTERIZER_DESC &desc, const char *name = nullptr);
  GraphicsObjectHandle create_blend_state(const TrackedLocation &loc, const D3D11_BLEND_DESC &desc, const char *name = nullptr);
  GraphicsObjectHandle create_depth_stencil_state(const TrackedLocation &loc, const D3D11_DEPTH_STENCIL_DESC &desc, const char *name = nullptr);
  GraphicsObjectHandle create_sampler(const TrackedLocation &loc, const std::string &name, const D3D11_SAMPLER_DESC &desc);

  D3D_FEATURE_LEVEL feature_level() const { return _feature_level; }

  GraphicsObjectHandle get_temp_render_target(const TrackedLocation &loc, int width, int height, DXGI_FORMAT format, uint32 bufferFlags, const std::string &name);
  void release_temp_render_target(GraphicsObjectHandle h);

  GraphicsObjectHandle create_render_target(const TrackedLocation &loc, int width, int height, DXGI_FORMAT format, uint32 bufferFlags, const std::string &name);
  GraphicsObjectHandle create_structured_buffer(const TrackedLocation &loc, int elemSize, int numElems, bool createSrv);
  GraphicsObjectHandle create_texture(const TrackedLocation &loc, const D3D11_TEXTURE2D_DESC &desc, const char *name);
  GraphicsObjectHandle get_texture(const char *filename);

  bool read_texture(const char *filename, D3DX11_IMAGE_INFO *info, uint32 *pitch, vector<uint8> *bits);

  void copy_resource(GraphicsObjectHandle dst, GraphicsObjectHandle src);

  // Create a texture, and fill it with data
  bool create_texture(const TrackedLocation &loc, int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, TextureResource *out);
  GraphicsObjectHandle create_texture(const TrackedLocation &loc, int width, int height, DXGI_FORMAT fmt, void *data, int data_width, int data_height, int data_pitch, const char *friendly_name);

  float fps() const { return _fps; }
  int width() const { return _width; }
  int height() const { return _height; }

  bool load_techniques(const char *filename, bool add_materials);
  Technique *get_technique(GraphicsObjectHandle h);
  GraphicsObjectHandle find_technique(const std::string &name);
  GraphicsObjectHandle find_resource(const std::string &name);
  GraphicsObjectHandle find_sampler(const std::string &name);
  GraphicsObjectHandle find_blend_state(const std::string &name);
  GraphicsObjectHandle find_rasterizer_state(const std::string &name);
  GraphicsObjectHandle find_depth_stencil_state(const std::string &name);

  GraphicsObjectHandle default_render_target() const { return _default_render_target; }

  ID3D11ClassLinkage *get_class_linkage();

  void add_shader_flag(const std::string &flag);
  int get_shader_flag(const std::string &flag);

  GraphicsObjectHandle default_rasterizer_state() const { return _default_rasterizer_state; }
  GraphicsObjectHandle default_depth_stencil_state() const { return _default_depth_stencil_state; }
  uint32_t default_stencil_ref() const { return 0; }
  GraphicsObjectHandle  default_blend_state() const { return _default_blend_state; }
  const float *default_blend_factors() const { return _default_blend_factors; }
  uint32_t default_sample_mask() const { return 0xffffffff; }

  DeferredContext *create_deferred_context(bool can_use_immediate);
  void destroy_deferred_context(DeferredContext *ctx);
  void add_command_list(ID3D11CommandList *cmd_list);

  bool vsync() const { return _vsync; }
  void set_vsync(bool value) { _vsync = value; }

  static GraphicsObjectHandle make_goh(GraphicsObjectHandle::Type type, int idx);

  void setDisplayAllModes(bool value) { _displayAllModes = value; }
  bool displayAllModes() const { return _displayAllModes; }

  const DXGI_MODE_DESC &selectedDisplayMode() const;

//private:
  Graphics();

  bool createWindow(WNDPROC wndProc);
  void setClientSize();

  bool create_buffer_inner(const TrackedLocation &loc, D3D11_BIND_FLAG bind, int size, bool dynamic, const void* data, ID3D11Buffer** buffer);

  bool create_render_target(const TrackedLocation &loc, int width, int height, DXGI_FORMAT format, uint32 bufferFlags, RenderTargetResource *out);
  bool create_texture(const TrackedLocation &loc, const D3D11_TEXTURE2D_DESC &desc, TextureResource *out);

  bool create_back_buffers(int width, int height);
  bool create_default_geometry();

  bool technique_file_changed(const char *filename, void *token);
  bool shader_file_changed(const char *filename, void *token);

  // given texture data and a name, insert it into the GOH chain
  GraphicsObjectHandle insert_texture(TextureResource *data, const char *friendly_name);

  void fill_system_resource_views(const ResourceViewArray &props, TextureArray *out) const;

  GraphicsObjectHandle load_texture(const char *filename, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info);
  GraphicsObjectHandle load_texture_from_memory(const void *buf, size_t len, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info);


  static INT_PTR CALLBACK dialogWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static bool enumerateDisplayModes(HWND hWnd);

  struct VideoAdapter {
    CComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC desc;
    vector<DXGI_MODE_DESC> displayModes;
  };

  struct Setup {

    Setup() : selectedAdapter(-1), selectedDisplayMode(-1), multisampleCount(-1), selectedAspectRatio(-1), windowed(false) {}

    std::vector<VideoAdapter> videoAdapters;
    int selectedAdapter;
    int selectedDisplayMode;
    int selectedAspectRatio;
    int multisampleCount;
    bool windowed;

  } _curSetup;


  // resources
  enum { IdCount = 1 << GraphicsObjectHandle::cIdBits };
  SearchableIdBuffer<std::string, ID3D11VertexShader *, IdCount> _vertex_shaders;
  SearchableIdBuffer<std::string, ID3D11PixelShader *, IdCount> _pixel_shaders;
  SearchableIdBuffer<std::string, ID3D11ComputeShader *, IdCount> _compute_shaders;
  SearchableIdBuffer<std::string, ID3D11GeometryShader *, IdCount> _geometry_shaders;
  IdBuffer<ID3D11InputLayout *, IdCount> _input_layouts;
  IdBuffer<ID3D11Buffer *, IdCount> _vertex_buffers;
  IdBuffer<ID3D11Buffer *, IdCount> _index_buffers;
  IdBuffer<ID3D11Buffer *, IdCount> _constant_buffers;
  SearchableIdBuffer<std::string, Technique *, IdCount> _techniques;

  SearchableIdBuffer<std::string, ID3D11BlendState *, IdCount> _blend_states;
  SearchableIdBuffer<std::string, ID3D11DepthStencilState *, IdCount> _depth_stencil_states;
  SearchableIdBuffer<std::string, ID3D11RasterizerState *, IdCount> _rasterizer_states;
  SearchableIdBuffer<std::string, ID3D11SamplerState *, IdCount> _sampler_states;

  SearchableIdBuffer<std::string, TextureResource *, IdCount> _textures;
  SearchableIdBuffer<std::string, RenderTargetResource *, IdCount> _render_targets;
  SearchableIdBuffer<std::string, SimpleResource *, IdCount> _resources;
  IdBuffer<ID3D11ShaderResourceView *, IdCount> _shader_resource_views;
  IdBuffer<StructuredBuffer *, IdCount> _structured_buffers;

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

  GraphicsObjectHandle _default_rasterizer_state;
  GraphicsObjectHandle _default_depth_stencil_state;
  CComPtr<ID3D11SamplerState> _default_sampler_state;
  float _default_blend_factors[4];
  GraphicsObjectHandle _default_blend_state;

  DWORD _start_fps_time;
  int32_t _frame_count;
  float _fps;
  CComPtr<ID3D11DeviceContext> _immediate_context;

  CComPtr<ID3D11ClassLinkage> _class_linkage;

  const char *_vs_profile;
  const char *_ps_profile;
  const char *_cs_profile;
  const char *_gs_profile;

  bool _vsync;
  int _totalBytesAllocated;

  std::map<PredefinedGeometry, std::pair<GraphicsObjectHandle, GraphicsObjectHandle> > _predefined_geometry;

  std::map<std::string, int> _shader_flags;

  HWND _hwnd;
  HINSTANCE _hInstance;
  bool _displayAllModes;
};

#define GRAPHICS Graphics::instance()

#define GFX_create_buffer(bind, size, dynamic, data) GRAPHICS.create_buffer(FROM_HERE, bind, size, dynamic, data);

#endif
