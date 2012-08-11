#ifndef _APP_HPP_
#define _APP_HPP_

#include "graphics.hpp"
#include "threading.hpp"
#include "json_utils.hpp"
#if WITH_WEBSOCKETS
#include "websocket_server.hpp"
#endif

struct TweakableParameterBlock;

/*
repeat after me: directx is left-handed. z goes into the screen.
*/

class EffectBase;
class EffectWrapper;

struct Scene;

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

class App : public threading::GreedyThread
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

  void process_network_msg(SOCKET sender, const char *msg, int len);

  typedef std::function<void (const JsonValue::JsonValuePtr &)> cbParamChanged;

  void add_parameter_block(const TweakableParameterBlock &block, const cbParamChanged &onChanged);

private:
  DISALLOW_COPY_AND_ASSIGN(App);
  App();
  ~App();

  void send_stats(const JsonValue::JsonValuePtr &frame);
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

  std::map<std::string, std::pair<JsonValue::JsonValuePtr, cbParamChanged>> _parameterBlocks;

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
