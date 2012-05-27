#ifndef _APP_HPP_
#define _APP_HPP_

#include "graphics.hpp"
#include "threading.hpp"
#if WITH_WEBSOCKETS
#include "websocket_server.hpp"
#endif

using std::string;
using std::map;
using std::vector;

/*
repeat after me: directx is left-handed. z goes into the screen.
*/

class EffectBase;
class EffectWrapper;

struct Scene;

struct RenderStates {
  RenderStates() : blend_state(nullptr), depth_stencil_state(nullptr), 
                   rasterizer_state(nullptr), sampler_state(nullptr) {}
  D3D11_BLEND_DESC blend_desc;
  ID3D11BlendState *blend_state;

  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc;
  ID3D11DepthStencilState *depth_stencil_state;

  D3D11_RASTERIZER_DESC rasterizer_desc;
  ID3D11RasterizerState *rasterizer_state;

  D3D11_SAMPLER_DESC sampler_desc;
  ID3D11SamplerState *sampler_state;
};

#if WITH_GWEN
namespace Gwen {
  namespace Renderer {
    class Base;
  }
  namespace Controls {
    class Canvas;
    class StatusBar;
  }
  namespace Skin {
    class TexturedBase;
  }
  namespace Input {
    class Windows;
  }
}
#endif

class App : 
  public threading::GreedyThread
{
public:

  static App& instance();

  bool init(HINSTANCE hinstance);
  static bool close();

  void	tick();

  virtual UINT run(void *userdata);
  void on_idle();

  void debug_text(const char *fmt, ...);
  double frame_time() const { return _frame_time; }

private:
  DISALLOW_COPY_AND_ASSIGN(App);
  App();
  ~App();

  bool create_window();
  void set_client_size();
  void find_app_root();


  static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

protected:

  static App* _instance;
  EffectBase* _test_effect;
  HINSTANCE _hinstance;
  int32_t _width;
  int32_t _height;
  HWND _hwnd;
  int _dbg_message_count;

  double _frame_time;

  int _cur_camera;
  bool _draw_plane;
  string _app_root;
  int _ref_count;

#if WITH_GWEN
  std::unique_ptr<Gwen::Controls::StatusBar> _gwen_status_bar;
  std::unique_ptr<Gwen::Input::Windows> _gwen_input;
  std::unique_ptr<Gwen::Controls::Canvas> _gwen_canvas;
  std::unique_ptr<Gwen::Skin::TexturedBase> _gwen_skin;
  std::unique_ptr<Gwen::Renderer::Base> _gwen_renderer;
#endif

};

#define APP App::instance()

#endif
