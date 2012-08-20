#include "stdafx.h"
#include "app.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "file_utils.hpp"
#include "string_utils.hpp"
#include "demo_engine.hpp"
#include "technique.hpp"
#include "kumi_loader.hpp"
#include "scene.hpp"
#include "property_manager.hpp"
#include "resource_interface.hpp"
#include "material_manager.hpp"
#include "threading.hpp"
#include "kumi.hpp"
#include "profiler.hpp"
#include "animation_manager.hpp"
#include "bit_utils.hpp"

#include "test/ps3_background.hpp"
#include "test/scene_player.hpp"
#include "test/particle_test.hpp"
#include "test/spline_test.hpp"

#pragma comment(lib, "psapi.lib")

using namespace std;
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
  , _frame_time(0)
{
  find_app_root();
}

App::~App()
{
}

App& App::instance()
{
  if (_instance == NULL)
    _instance = new App();
  return *_instance;
}

bool App::init(HINSTANCE hinstance)
{
  if (_appRootFilename.empty()) {
    MessageBoxA(0, "Unable to find resource.dat", "Error", MB_ICONEXCLAMATION);
    return false;
  }

  LOGGER.
#if WITH_UNPACKED_RESOURCES
    open_output_file(_T("kumi.log")).
#endif
    break_on_error(true);

  _hinstance = hinstance;
  _width = 3 * GetSystemMetrics(SM_CXSCREEN) / 4;
  _height = 3 * GetSystemMetrics(SM_CYSCREEN) / 4;

  B_ERR_BOOL(ProfileManager::create());
  B_ERR_BOOL(PropertyManager::create());
#if WITH_UNPACKED_RESOUCES
  B_ERR_BOOL(ResourceManager::create("resources.log"));
#else
  B_ERR_BOOL(PackedResourceManager::create("resources.dat"));
#endif
  B_ERR_BOOL(Graphics::create());
  B_ERR_BOOL(MaterialManager::create());
  B_ERR_BOOL(DemoEngine::create());
#if WITH_WEBSOCKETS
  B_ERR_BOOL(WebSocketServer::create());
#endif
  B_ERR_BOOL(AnimationManager::create());

  B_ERR_BOOL(create_window());

#if WITH_UNPACKED_RESOUCES
  RESOURCE_MANAGER.add_path("D:\\SkyDrive");
  RESOURCE_MANAGER.add_path("c:\\users\\dooz\\SkyDrive");
  RESOURCE_MANAGER.add_path("C:\\Users\\dooz\\Dropbox");
  RESOURCE_MANAGER.add_path("D:\\Dropbox");
#endif

  //VolumetricEffect *effect = new VolumetricEffect(GraphicsObjectHandle(), "simple effect");
  //auto effect = new Ps3BackgroundEffect(GraphicsObjectHandle(), "funky background");
  //auto effect = new ScenePlayer(GraphicsObjectHandle(), "funky background");

  auto effect = new ScenePlayer("simple effect");
  //auto effect = new ParticleTest("particle test");
  //auto effect = new SplineTest("spline test");
  DEMO_ENGINE.add_effect(effect, 0, 10000 * 1000);

  load_settings();

  return true;
}

bool App::close() {

  B_ERR_BOOL(AnimationManager::close());

#if WITH_WEBSOCKETS
  B_ERR_BOOL(WebSocketServer::close());
#endif
#if WITH_UNPACKED_RESOUCES
  B_ERR_BOOL(FileWatcher::close());
#endif
  B_ERR_BOOL(DemoEngine::close());
  B_ERR_BOOL(Graphics::close());
  B_ERR_BOOL(MaterialManager::close());
#if WITH_UNPACKED_RESOUCES
  B_ERR_BOOL(ResourceManager::close());
#else
  B_ERR_BOOL(PackedResourceManager::close());
#endif
  B_ERR_BOOL(PropertyManager::close());
  B_ERR_BOOL(ProfileManager::close());

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

#if WITH_WEBSOCKETS

static void send_json(SOCKET socket, const JsonValue::JsonValuePtr &json) {
  string str = print_json2(json);
  char *buf = new char[str.size()];
  memcpy(buf, str.data(), str.size());
  WEBSOCKET_SERVER.send_msg(socket, buf, str.size());
}

void App::process_network_msg(SOCKET sender, const char *msg, int len) {

  if (strncmp(msg, "REQ:DEMO.INFO", len) == 0) {
    send_json(sender, DEMO_ENGINE.get_info());

  } else if (strncmp(msg, "REQ:PARAM.INFO", len) == 0) {
    auto root = JsonValue::create_object();
    auto blocks = root->add_key_value("blocks", JsonValue::create_array());
    for (auto it = begin(_parameterBlocks); it != end(_parameterBlocks); ++it) {
      blocks->add_value(it->second.first);
    }
    send_json(sender, root);

  } else {
    JsonValue::JsonValuePtr m = parse_json(msg, msg + len);
    auto inner = m->get("msg");
    if (!inner->is_null()) {
      // msg has type and data fields
      auto type = inner->get("type")->to_string();
      auto data = inner->get("data");

      if (type == "time") {
        bool playing = (*data)["is_playing"]->to_bool();
        int cur_time = (*data)["cur_time"]->to_int();
        DEMO_ENGINE.set_pos(cur_time);
        DEMO_ENGINE.set_paused(!playing);

      } else if (type == "demo") {
        DEMO_ENGINE.update(data);

      } else if (type == "STATE:PARAM") {
        // look up the callback for the given block
        auto blockName = data->get("name")->to_string();
        auto it = _parameterBlocks.find(blockName);
        if (it != _parameterBlocks.end()) {
          // update the paramblock with the new settings, and invoke the callback
          it->second.first = data;
          it->second.second(data->get("params"));
        } else {
          LOG_WARNING_LN("Unknown param block: %s", blockName.c_str());
        }
      }
    }
  }

  delete [] msg;
}

void App::send_stats(const JsonValue::JsonValuePtr &frame) {

  if (WEBSOCKET_SERVER.num_clients_connected() == 0)
    return;

  {
    auto root = JsonValue::create_object();
    auto obj = root->add_key_value("system.frame", JsonValue::create_object());

    // convert UTC time to Unix epoch
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    int64 now = (LONGLONG)time.dwLowDateTime + ((LONGLONG)(time.dwHighDateTime) << 32LL);
    now = (now / 10000) - 11644473600000;

    PROCESS_MEMORY_COUNTERS counters;
    ZeroMemory(&counters, sizeof(counters));
    counters.cb = sizeof(counters);
    GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));

    obj->add_key_value("timestamp", (double)now);
    obj->add_key_value("fps", JsonValue::create_number(GRAPHICS.fps()));
    obj->add_key_value("ms", JsonValue::create_number(APP.frame_time()));
    obj->add_key_value("mem", JsonValue::create_int(counters.WorkingSetSize / 1024));

    send_json(0, root);
  }

  send_json(0, frame);
}
#endif

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
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {

      PROFILE_MANAGER.start_frame();
      LARGE_INTEGER start, end;
      QueryPerformanceCounter(&start);
      {
        ADD_PROFILE_SCOPE();

        process_deferred();

        GRAPHICS.clear(XMFLOAT4(0,0,0,1));
        DEMO_ENGINE.tick();
      }
      {
        ADD_PROFILE_SCOPE();
        GRAPHICS.present();

        on_idle();
      }

      QueryPerformanceCounter(&end);
      int64 delta = end.QuadPart - start.QuadPart;
      double cur_frame = delta / (double)freq.QuadPart;
      _frame_time = lerp(cur_frame, _frame_time, 0.6);

      JsonValue::JsonValuePtr frame = PROFILE_MANAGER.end_frame();
#if WITH_WEBSOCKETS
      // limit how often we send the profile data
      static DWORD lastTime = timeGetTime();
      DWORD now = timeGetTime();
      if (now - lastTime > 100) {
        send_stats(frame);
        lastTime = now;
      }
#endif

    }
  }
  return 0;
}

void App::load_settings() {
  if (_appRootFilename.empty())
    return;

  vector<char> appRoot;
  if (!RESOURCE_MANAGER.load_file(_appRootFilename.c_str(), &appRoot))
    return;

  auto appSettings = parse_json(appRoot.data(), appRoot.data() + appRoot.size());
  if (!appSettings->has_key("curSettings"))
    return;

  string curSettings = appSettings->get("curSettings")->to_string();
  if (!appSettings->has_key(curSettings))
    return;

  auto settings = appSettings->get(curSettings);

  // init the params
  auto params = settings->get("params");
  for (int i = 0; i < params->count(); ++i) {
    auto cur = params->get(i);
    string name = cur->get("name")->to_string();
    auto it = _parameterBlocks.find(name);
    if (it != _parameterBlocks.end()) {
      it->second.second(cur->get("params"));
    }
  }
}

void App::save_settings() {

  if (_appRootFilename.empty())
    return;

  vector<char> appRoot;
  if (!RESOURCE_MANAGER.load_file(_appRootFilename.c_str(), &appRoot))
    return;

  auto appSettings = parse_json(appRoot.data(), appRoot.data() + appRoot.size());
  if (appSettings->is_null())
    appSettings = JsonValue::create_object();

  // create a timestamp based name for the settings
  time_t rawTime;
  tm timeInfo;
  time(&rawTime);
  localtime_s(&timeInfo, &rawTime);

  char now[80];
  strftime(now, sizeof(now), "settings_%Y_%m_%d_%H_%M_%S", &timeInfo);

  auto settings = JsonValue::create_object();
  auto params = settings->add_key_value("params", JsonValue::create_array());

  for (auto it = begin(_parameterBlocks); it != end(_parameterBlocks); ++it) {
    params->add_value(it->second.first);
  }

  appSettings->add_key_value("curSettings", now);
  appSettings->add_key_value(now, settings);

  string json = print_json(appSettings);
  save_file(_appRootFilename.c_str(), json.c_str(), json.size());
}

LRESULT App::wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

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

      case 'S':
        if (is_bit_set(GetKeyState(VK_CONTROL), 15)) {
#if WITH_UNPACKED_RESOUCES
          APP.save_settings();
#endif
          return 0;
        }
        break;

      case 'V':
        GRAPHICS.set_vsync(!GRAPHICS.vsync());
        return 0;

      case 'M': {
#if _DEBUG
        static _CrtMemState prevState;
        static bool firstTime = true;
        _CrtMemState curState;
        _CrtMemCheckpoint(&curState);

        if (!firstTime) {
          _CrtMemState stateDiff;
          OutputDebugStringA("*** MEM DIFF ***\n");
          _CrtMemDifference(&stateDiff, &prevState, &curState);
          _CrtMemDumpStatistics(&stateDiff);

        }
        firstTime = false;
        OutputDebugStringA("*** MEM STATS ***\n");
        _CrtMemDumpStatistics(&curState);
        prevState = curState;
#endif
        return 0;
      }

      case VK_ESCAPE:
        PostQuitMessage( 0 );
        return 0;
    
      case VK_SPACE:
        DEMO_ENGINE.set_paused(!DEMO_ENGINE.paused());
        return 0;

      case VK_PRIOR:
        DEMO_ENGINE.set_pos(DEMO_ENGINE.pos() - 1000);
        return 0;

      case VK_NEXT:
        DEMO_ENGINE.set_pos(DEMO_ENGINE.pos() + 1000);
        return 0;

      case VK_LEFT:
        DEMO_ENGINE.set_pos(DEMO_ENGINE.pos() - 100);
        return 0;

      case VK_RIGHT:
        DEMO_ENGINE.set_pos(DEMO_ENGINE.pos() + 100);
        return 0;

      case VK_HOME:
        DEMO_ENGINE.set_pos(0);
        return 0;

      default:
        break;
      }
    break;

  }

  DEMO_ENGINE.wnd_proc(message, wParam, lParam);

  return DefWindowProc( hWnd, message, wParam, lParam );
}

void App::find_app_root()
{
  char starting_dir[MAX_PATH];
  _getcwd(starting_dir, MAX_PATH);

  // keep going up directory levels until we find "app.json", or we hit the bottom..
  char prev_dir[MAX_PATH], cur_dir[MAX_PATH];
  ZeroMemory(prev_dir, sizeof(prev_dir));

  while (true) {
    _getcwd(cur_dir, MAX_PATH);
    // check if we haven't moved
    if (!strcmp(cur_dir, prev_dir))
      break;

    memcpy(prev_dir, cur_dir, MAX_PATH);

#if WITH_UNPACKED_RESOUCES
    if (file_exists("app.json")) {
      _app_root = cur_dir;
      _appRootFilename = "app.json";
      return;
    }
#else
    if (file_exists("resources.dat")) {
      _app_root = cur_dir;
      _appRootFilename = "app.json";
      return;
    }
#endif
    if (_chdir("..") == -1)
      break;
  }
  _app_root = starting_dir;
}

void App::add_parameter_block(const TweakableParameterBlock &paramBlock, bool initialCallback, const cbParamChanged &onChanged) {
  // create a json rep for the parameter block
  auto root = JsonValue::create_object();
  auto block = JsonValue::create_object();
  auto params = JsonValue::create_object();
  root->add_key_value("block", block);
  block->add_key_value("name", paramBlock._blockName);
  block->add_key_value("params", params);

  for (auto it = begin(paramBlock._params); it != end(paramBlock._params); ++it) {

    auto param = *it;
    auto curParam = JsonValue::create_object();

    if (param.type() == TweakableParameter::kTypeFloat) {
      curParam->add_key_value("value", JsonValue::create_number(param.floatValue()));
      if (param.isBounded()) {
        curParam->add_key_value("minValue", JsonValue::create_number(param.floatMin()));
        curParam->add_key_value("maxValue", JsonValue::create_number(param.floatMax()));
      }

    } else {
      LOG_ERROR_LN("Implement me");
    }
    params->add_key_value(param.name(), curParam);
  }

  if (initialCallback)
    onChanged(params);

  _parameterBlocks[paramBlock._blockName] = make_pair(block, onChanged);
}
