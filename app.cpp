#include "stdafx.h"
#include "app.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "effect_wrapper.hpp"
#include "file_utils.hpp"
#include "string_utils.hpp"
#include "demo_engine.hpp"
#include "technique.hpp"
#include "kumi_loader.hpp"
#include "scene.hpp"
#include "property_manager.hpp"
#include "resource_manager.hpp"
#include "material_manager.hpp"
#include "renderer.hpp"
#include "threading.hpp"
#include "kumi.hpp"

#include "test/demo.hpp"
#include "test/path_follow.hpp"
#include "test/volumetric.hpp"
#include "test/ps3_background.hpp"
#include "test/scene_player.hpp"

#if WITH_GWEN
#include "kumi_gwen.hpp"
#include "gwen/Skins/TexturedBase.h"
#include "gwen/Controls/Canvas.h"
#include "gwen/Controls/Button.h"
#include "gwen/Input/Windows.h"
#include "gwen/UnitTest/UnitTest.h"
#include "gwen/Controls/StatusBar.h"
#endif

using std::swap;
using namespace threading;

App* App::_instance = nullptr;

const TCHAR *g_app_window_class = _T("KumiClass");
const TCHAR *g_app_window_title = _T("kumi - magnus österlind");

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	((int)(short)HIWORD(lParam))
#endif

App::App()
  : GreedyThread(threading::kMainThread)
  , _hinstance(NULL)
  , _width(-1)
  , _height(-1)
  , _hwnd(NULL)
  , _test_effect(NULL)
  , _dbg_message_count(0)
  , _cur_camera(1)
  , _draw_plane(false)
  , _ref_count(1)
  , _frame_time(0)
{
  find_app_root();
}

App::~App()
{
#if WITH_GWEN
  _gwen_status_bar.reset();
  _gwen_input.reset();
  _gwen_canvas.reset();
  _gwen_skin.reset();
  _gwen_renderer.reset();
#endif
}

App& App::instance()
{
  if (_instance == NULL)
    _instance = new App();
  return *_instance;
}

bool App::init(HINSTANCE hinstance)
{
  LOGGER.
    open_output_file(_T("kumi.log")).
    break_on_error(true);

  _hinstance = hinstance;
  _width = 3 * GetSystemMetrics(SM_CXSCREEN) / 4;
  _height = 3 * GetSystemMetrics(SM_CYSCREEN) / 4;

  B_ERR_BOOL(PropertyManager::create());
  B_ERR_BOOL(ResourceManager::create());
  B_ERR_BOOL(Graphics::create(&RESOURCE_MANAGER));
  B_ERR_BOOL(Renderer::create());
  B_ERR_BOOL(MaterialManager::create());
  B_ERR_BOOL(DemoEngine::create());
#if WITH_WEBSOCKETS
  B_ERR_BOOL(WebSocketServer::create());
#endif

  B_ERR_BOOL(create_window());

#if WITH_GWEN
  _gwen_renderer.reset(create_kumi_gwen_renderer());
  _gwen_skin.reset(new Gwen::Skin::TexturedBase(_gwen_renderer.get()));
  _gwen_skin->Init("gfx/DefaultSkin.png");
  _gwen_canvas.reset(new Gwen::Controls::Canvas(_gwen_skin.get()));
  _gwen_canvas->SetSize(GRAPHICS.width(), GRAPHICS.height());
  _gwen_input.reset(new Gwen::Input::Windows());
  _gwen_input->Initialize(_gwen_canvas.get());
  _gwen_status_bar.reset(new Gwen::Controls::StatusBar(_gwen_canvas.get()));
#endif

  RESOURCE_MANAGER.add_path("D:\\SkyDrive");
  RESOURCE_MANAGER.add_path("c:\\users\\dooz\\SkyDrive");
  RESOURCE_MANAGER.add_path("C:\\Users\\dooz\\Dropbox");
  RESOURCE_MANAGER.add_path("D:\\Dropbox");

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/gwen.tec", true));
/*
  if (!GRAPHICS.load_techniques("effects/cef.tec", true)) {
    // log error
  }
*/
  //VolumetricEffect *effect = new VolumetricEffect(GraphicsObjectHandle(), "simple effect");
  //auto effect = new Ps3BackgroundEffect(GraphicsObjectHandle(), "funky background");
  //auto effect = new ScenePlayer(GraphicsObjectHandle(), "funky background");
  auto effect = new ScenePlayer(GraphicsObjectHandle(), "simple effect");
  DEMO_ENGINE.add_effect(effect, 0, 7 * 1000);

  return true;
}

bool App::close() {

#if WITH_WEBSOCKETS
  B_ERR_BOOL(WebSocketServer::close());
#endif
  B_ERR_BOOL(FileWatcher::close());
  B_ERR_BOOL(DemoEngine::close());
  B_ERR_BOOL(Renderer::close());
  B_ERR_BOOL(Graphics::close());
  B_ERR_BOOL(MaterialManager::close());
  B_ERR_BOOL(ResourceManager::close());
  B_ERR_BOOL(PropertyManager::close());
  Logger::close();
  Dispatcher::close();
  delete exch_null(_instance);
  return true;
}

void App::set_client_size()
{
  RECT client_rect;
  RECT window_rect;
  GetClientRect(_hwnd, &client_rect);
  GetWindowRect(_hwnd, &window_rect);
  window_rect.right -= window_rect.left;
  window_rect.bottom -= window_rect.top;
  window_rect.left = window_rect.top = 0;
  const int dx = window_rect.right - client_rect.right;
  const int dy = window_rect.bottom - client_rect.bottom;

  SetWindowPos(_hwnd, NULL, -1, -1, _width + dx, _height + dy, SWP_NOZORDER | SWP_NOREPOSITION);
}

bool App::create_window()
{
  WNDCLASSEX wcex;
  ZeroMemory(&wcex, sizeof(wcex));

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style          = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc    = wnd_proc;
  wcex.hInstance      = _hinstance;
  wcex.hbrBackground  = 0;
  wcex.lpszClassName  = g_app_window_class;

  B_ERR_INT(RegisterClassEx(&wcex));

  //const UINT window_style = WS_VISIBLE | WS_POPUP | WS_OVERLAPPEDWINDOW;
  const UINT window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS; //WS_VISIBLE | WS_POPUP;

  _hwnd = CreateWindow(g_app_window_class, g_app_window_title, window_style,
    CW_USEDEFAULT, CW_USEDEFAULT, _width, _height, NULL, NULL,
    _hinstance, NULL);

  set_client_size();

  ShowWindow(_hwnd, SW_SHOW);

  return true;
}

void App::on_idle() {
}

UINT App::run(void *userdata) {
  MSG msg = {0};

  float running_time = 0;
  const float dt = 1 / 100.0f;

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  LARGE_INTEGER cur;
  QueryPerformanceCounter(&cur);
  float cur_time = (float)(cur.QuadPart / freq.QuadPart);
  float accumulator = 0;

  DEMO_ENGINE.start();

  while (WM_QUIT != msg.message) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ) {
#if WITH_GWEN
      _gwen_input->ProcessMessage(msg);
#endif
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {

      LARGE_INTEGER start, end;
      QueryPerformanceCounter(&start);

      process_deferred();

      GRAPHICS.clear(XMFLOAT4(0,0,0,1));
      DEMO_ENGINE.tick();

#if WITH_GWEN
      XMFLOAT4 tt;
      PROPERTY_MANAGER.get_system_property("g_time", &tt);
      _gwen_status_bar->SetText(Gwen::Utility::Format(L"fps: %.2f, %.2f, %.2f", GRAPHICS.fps(), tt.x, tt.y));
      _gwen_canvas->RenderCanvas();
#endif
      RENDERER.render();
      GRAPHICS.present();

      QueryPerformanceCounter(&end);
      int64 delta = end.QuadPart - start.QuadPart;
      double cur_frame = delta / (double)freq.QuadPart;
      if (_frame_time > 0) {
        double p = 0.4;
        _frame_time = p * _frame_time + (1-p) * cur_frame;
      } else {
        _frame_time = cur_frame;
      }

    }
  }
  return 0;
}

LRESULT App::wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

  DEMO_ENGINE.wnd_proc(message, wParam, lParam);

  switch( message ) {

  case WM_SETFOCUS:
  case WM_KILLFOCUS:
    {
      SetFocus(_instance->_hwnd);
    }
    return 0;

  case WM_SIZE:
    GRAPHICS.resize(LOWORD(lParam), HIWORD(lParam));
    break;

  case WM_APP_CLOSE:
    DestroyWindow(hWnd);
    break;

  case WM_CREATE:
    {
      _instance->_hwnd = hWnd;
      B_ERR_BOOL(GRAPHICS.init(hWnd, _instance->_width, _instance->_height));
    }
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  case WM_LBUTTONUP:
  case WM_MBUTTONUP:
  case WM_RBUTTONUP:
    break;

  case WM_LBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONDOWN:
    break;

  case WM_MOUSEMOVE:
    break;

  case WM_MOUSEWHEEL:
    break;

  case WM_KEYUP:
    switch (wParam) {
      case VK_ESCAPE:
        PostQuitMessage( 0 );
        break;
      default:
        break;
      }
    break;

  }
  return DefWindowProc( hWnd, message, wParam, lParam );
}

void App::find_app_root()
{
  // keep going up directory levels until we find "app.root", or we hit the bottom..
  char starting_dir[MAX_PATH], prev_dir[MAX_PATH], cur_dir[MAX_PATH];
  ZeroMemory(prev_dir, sizeof(prev_dir));

  _getcwd(starting_dir, MAX_PATH);
  while (true) {
    _getcwd(cur_dir, MAX_PATH);
    // check if we haven't moved
    if (!strcmp(cur_dir, prev_dir))
      break;

    memcpy(prev_dir, cur_dir, MAX_PATH);

    if (file_exists("app.root")) {
      _app_root = cur_dir;
      return;
    }
    if (_chdir("..") == -1)
      break;
  }
  _app_root = starting_dir;
}

