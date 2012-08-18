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

#if WITH_WEBSOCKETS
  void process_network_msg(SOCKET sender, const char *msg, int len);
#endif

  typedef std::function<void (const JsonValue::JsonValuePtr &)> cbParamChanged;

  void add_parameter_block(const TweakableParameterBlock &block, bool initialCallback, const cbParamChanged &onChanged);

private:
  DISALLOW_COPY_AND_ASSIGN(App);
  App();
  ~App();

#if WITH_WEBSOCKETS
  void send_stats(const JsonValue::JsonValuePtr &frame);
#endif
  bool create_window();
  void set_client_size();
  void find_app_root();

  void save_settings();
  void load_settings();

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

  std::string _app_root;
  std::string _appRootFilename;

  std::map<std::string, std::pair<JsonValue::JsonValuePtr, cbParamChanged>> _parameterBlocks;

};

#define APP App::instance()

#endif
