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

#if WITH_CEF
#include "v8_handler.hpp"
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

#if WITH_CEF
#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")

#define REQUIRE_UI_THREAD()

void App::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const CefRect& dirtyRect, const void* buffer) {
  D3D11_MAPPED_SUBRESOURCE sub;
  GRAPHICS.map(_cef_staging, 0, D3D11_MAP_WRITE, 0, &sub);
  uint8_t *dst = (uint8_t *)sub.pData + 4 * dirtyRect.x + dirtyRect.y * sub.RowPitch;
  uint8_t *src = (uint8_t *)buffer + 4 * (dirtyRect.x + dirtyRect.y * _width);
  const int src_pitch = _width * 4;
  for (int i = 0; i < dirtyRect.height; ++i) {
    memcpy(dst, src, src_pitch);
    dst += sub.RowPitch;
    src += src_pitch;
  }
  GRAPHICS.unmap(_cef_staging, 0);
  GRAPHICS.copy_resource(_cef_texture, _cef_staging);
}

void App::OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor)
{
  REQUIRE_UI_THREAD();

  // Change the plugin window's cursor.
  SetClassLong(_hwnd, GCL_HCURSOR, static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
  SetCursor(cursor);
}

bool App::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
  REQUIRE_UI_THREAD();

  // The simulated screen and view rectangle are the same. This is necessary
  // for popup menus to be located and sized inside the view.
  rect.x = rect.y = 0;
  rect.width = _width;
  rect.height = _height;
  return true;
}

bool App::GetScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
  return GetViewRect(browser, rect);
}

bool App::GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX, int viewY, int& screenX, int& screenY)
{
  REQUIRE_UI_THREAD();

  // Convert the point from view coordinates to actual screen coordinates.
  POINT screen_pt = {viewX, viewY};
  MapWindowPoints(_hwnd, HWND_DESKTOP, &screen_pt, 1);
  screenX = screen_pt.x;
  screenY = screen_pt.y;
  return true;
}


void App::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
  REQUIRE_UI_THREAD();

  AutoLock lock_scope(this);
  if(!browser->IsPopup())
  {
    // We need to keep the main child window, but not popup windows
    m_Browser = browser;
    _browser_hwnd = browser->GetWindowHandle();
    browser->SetSize(PET_VIEW, _width, _height);
  }
}

void App::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
  REQUIRE_UI_THREAD();

  if(_browser_hwnd == browser->GetWindowHandle()) {
    // Free the browser pointer so that the browser can be destroyed
    m_Browser = NULL;
  }
}

void App::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame)
{
  REQUIRE_UI_THREAD();

  if(!browser->IsPopup() && frame->IsMain()) {
    // We've just started loading a page
    SetLoading(true);
  }
}

void App::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
  REQUIRE_UI_THREAD();

  if(!browser->IsPopup() && frame->IsMain()) {
    // We've just finished loading a page
    SetLoading(false);

    CefRefPtr<CefDOMVisitor> visitor = GetDOMVisitor(frame->GetURL());
    if(visitor.get())
      frame->VisitDOM(visitor);
  }
}

bool App::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, 
                      const CefString& failedUrl, CefString& errorText)
{
  REQUIRE_UI_THREAD();

  if(errorCode == ERR_CACHE_MISS) {
    // Usually caused by navigating to a page with POST data via back or
    // forward buttons.
    errorText = "<html><head><title>Expired Form Data</title></head>"
      "<body><h1>Expired Form Data</h1>"
      "<h2>Your form request has expired. "
      "Click reload to re-submit the form data.</h2></body>"
      "</html>";
  } else {
    // All other messages.
    std::stringstream ss;
    ss <<       "<html><head><title>Load Failed</title></head>"
      "<body><h1>Load Failed</h1>"
      "<h2>Load of URL " << std::string(failedUrl) <<
      " failed with error code " << static_cast<int>(errorCode) <<
      ".</h2></body>"
      "</html>";
    errorText = ss.str();
  }

  return false;
}

std::string App::GetLogFile()
{
  AutoLock lock_scope(this);
  return m_LogFile;
}

void App::SetLastDownloadFile(const std::string& fileName)
{
  AutoLock lock_scope(this);
  m_LastDownloadFile = fileName;
}

std::string App::GetLastDownloadFile()
{
  AutoLock lock_scope(this);
  return m_LastDownloadFile;
}

void App::AddDOMVisitor(const std::string& path, CefRefPtr<CefDOMVisitor> visitor)
{
  AutoLock lock_scope(this);
  DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
  if (it == m_DOMVisitors.end())
    m_DOMVisitors.insert(std::make_pair(path, visitor));
  else
    it->second = visitor;
}

CefRefPtr<CefDOMVisitor> App::GetDOMVisitor(const std::string& path)
{
  AutoLock lock_scope(this);
  DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
  if (it != m_DOMVisitors.end())
    return it->second;
  return NULL;
}

bool App::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser, const CefPopupFeatures& popupFeatures, 
                        CefWindowInfo& windowInfo, const CefString& url, CefRefPtr<CefClient>& client, 
                        CefBrowserSettings& settings)
{
  REQUIRE_UI_THREAD();

  return false;
}


void App::SetLoading(bool isLoading)
{
  //ASSERT(m_EditHwnd != NULL && m_ReloadHwnd != NULL && m_StopHwnd != NULL);
  EnableWindow(_edit_hwnd, TRUE);
  EnableWindow(_reload_hwnd, !isLoading);
  EnableWindow(_stop_hwnd, isLoading);
}

void App::SetNavState(bool canGoBack, bool canGoForward)
{
  //ASSERT(m_BackHwnd != NULL && m_ForwardHwnd != NULL);
  EnableWindow(_back_hwnd, canGoBack);
  EnableWindow(_forward_hwnd, canGoForward);
}
#endif // #if WITH_CEF

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
#if WITH_CEF
  , _main_hwnd(NULL)
  , _browser_hwnd(NULL)
  , _edit_hwnd(NULL)
  , _back_hwnd(NULL)
  , _forward_hwnd(NULL)
  , _stop_hwnd(NULL)
  , _reload_hwnd(NULL)
#endif
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
  _width = GetSystemMetrics(SM_CXSCREEN) / 2;
  _height = GetSystemMetrics(SM_CYSCREEN) / 2;

#if WITH_CEF
  CefSettings settings;
  settings.multi_threaded_message_loop = false;
#endif

  B_ERR_BOOL(ResourceManager::create());
  B_ERR_BOOL(Graphics::create(&RESOURCE_MANAGER));
  B_ERR_BOOL(Renderer::create());

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
  RESOURCE_MANAGER.add_path("C:\\syncplicity");
  RESOURCE_MANAGER.add_path("C:\\Users\\dooz\\Dropbox");
  RESOURCE_MANAGER.add_path("D:\\Dropbox");
  RESOURCE_MANAGER.add_path("D:\\syncplicity");

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/gwen.tec", true));
/*
  if (!GRAPHICS.load_techniques("effects/cef.tec", true)) {
    // log error
  }
*/
  //VolumetricEffect *effect = new VolumetricEffect(GraphicsObjectHandle(), "simple effect");
  //auto effect = new Ps3BackgroundEffect(GraphicsObjectHandle(), "funky background");
  auto effect = new ScenePlayer(GraphicsObjectHandle(), "funky background");
  DEMO_ENGINE.add_effect(effect, 0, 1000 * 1000);

  B_ERR_BOOL(DEMO_ENGINE.init());

  return true;
}

bool App::close() {
  FileWatcher::close();
  Renderer::close();
  Graphics::close();
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
      process_deferred();

#if WITH_CEF
      CefDoMessageLoopWork();
#endif

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
    }
  }
  return 0;
}

/*
int funky(int a, float b, CefString c) {
  return 42;
}

CefString monkey(CefString c) {
  return CefString("apa");
}

void monkey2(int a) {

}


struct something {
  something() : a(20) {}
  int inner(bool) {
    return 10;
  }
  int a;
};
*/

/*
void InitExtensionTest()
{

//make_funky(&something::inner);

something *a = new something;
//auto x = boost::bind(&something::inner, a);
//make_funky(x);

MyV8Handler *vv = new MyV8Handler;

vv->AddFunction("funky", "funky", funky);
vv->AddFunction("funky2", "funky", funky);
vv->AddFunction("monkey", "monkey", monkey);
vv->AddFunction("monkey2", "monkey2", monkey2);

vv->AddFunction("xxx", "xxx", a, &something::inner);
vv->AddVariable("apa", &apa);
vv->AddVariable("apa2", &a->a);
vv->Register();
}
*/
LRESULT App::wnd_proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{

  //HDC hdc;
  //PAINTSTRUCT ps;
  switch( message ) 
  {

  case WM_SETFOCUS:
  case WM_KILLFOCUS:
    {
      SetFocus(_instance->_hwnd);
#if WITH_CEF
      if (GetBrowser())
        GetBrowser()->SetFocus(message == WM_SETFOCUS);
#endif
    }
    return 0;

  case WM_SIZE:
    GRAPHICS.resize(LOWORD(lParam), HIWORD(lParam));
#if WITH_CEF
    _cef_texture = GRAPHICS.create_texture(
      CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, _width, _height, 1, 1, D3D11_BIND_SHADER_RESOURCE, 
      D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE),
      "cef_texture");

    _cef_staging = GRAPHICS.create_texture(
      CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, _width, _height, 1, 1, 0, D3D11_USAGE_STAGING, 
      D3D11_CPU_ACCESS_WRITE),
      "cef_staging");

    if (GetBrowserHwnd()) {
      RECT rect;
      GetClientRect(hWnd, &rect);
      HDWP hdwp = BeginDeferWindowPos(1);
      hdwp = DeferWindowPos(hdwp, GetBrowserHwnd(), 0, rect.left, rect.top, rect.right - rect.left, 
        rect.bottom - rect.top, SWP_NOZORDER);
      EndDeferWindowPos(hdwp);
      GetBrowser()->SetSize(PET_VIEW, rect.right - rect.left, rect.bottom - rect.top);
    }
#endif
    break;

  case WM_APP_CLOSE:
    DestroyWindow(hWnd);
    break;

  //case WM_ERASEBKGND:
    //return 0;
/*
  case WM_PAINT:
    hdc = BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    return 0;
*/
  case WM_CREATE:
    {
      _instance->_hwnd = hWnd;
      B_ERR_BOOL(GRAPHICS.init(hWnd, _instance->_width, _instance->_height));
#if WITH_CEF
      CefWindowInfo info;
      info.m_bIsTransparent = TRUE;
      CefBrowserSettings settings;

      RECT rect;
      GetClientRect(hWnd, &rect);
      info.SetAsOffScreen(hWnd);
      info.SetIsTransparent(TRUE);
      CefBrowser::CreateBrowser(info, CefRefPtr<CefClient>(this), "file://C:/temp/tjong.html", settings);
#endif
      //InitExtensionTest();
      //CefBrowser::CreateBrowser(info, static_cast<CefRefPtr<CefClient> >(this), "http://www.google.com", settings);
    }
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  case WM_LBUTTONUP:
  case WM_MBUTTONUP:
  case WM_RBUTTONUP:
#if WITH_CEF
    if (GetBrowser()) {
      if (GetCapture() == hWnd)
        ReleaseCapture();
      cef_mouse_button_type_t btn = message == WM_LBUTTONUP ? MBT_LEFT : (message == WM_MBUTTONUP ? MBT_MIDDLE : MBT_RIGHT);
      GetBrowser()->SendMouseClickEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), btn, true, 1);
    }
#endif
    break;

  case WM_LBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONDOWN:
#if WITH_CEF
    if (GetBrowser()) {
      SetCapture(hWnd);
      SetFocus(hWnd);
      cef_mouse_button_type_t btn = message == WM_LBUTTONDOWN ? MBT_LEFT : (message == WM_MBUTTONDOWN ? MBT_MIDDLE : MBT_RIGHT);
      GetBrowser()->SendMouseClickEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), btn, false, 1);
    }
#endif
    break;

  case WM_MOUSEMOVE:
#if WITH_CEF
    if (GetBrowser())
      GetBrowser()->SendMouseMoveEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
#endif
    break;

  case WM_MOUSEWHEEL:
    break;
    /*
    case WM_KEYDOWN:
    break;

    case WM_CHAR:
    break;

    case WM_KEYUP:
    switch (wParam) 
    {
    case VK_ESCAPE:
    PostQuitMessage( 0 );
    break;
    default:
    break;
    }
    break;
    */

  case WM_KEYUP:
    switch (wParam) {
      case VK_F5:
        DEMO_ENGINE.reset_current_effect();
        break;
    }
    break;

  case WM_KEYDOWN:
  //case WM_KEYUP:
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_CHAR:
  case WM_SYSCHAR:
  case WM_IME_CHAR:
#if WITH_CEF
    if (GetBrowser()) {
      const CefBrowser::KeyType type = 
        (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) ? KT_KEYDOWN :
        (message == WM_KEYUP || message == WM_SYSKEYUP) ? KT_KEYUP :
        KT_CHAR;

      const bool sys_char = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SYSCHAR;
      const bool ime_char = message == WM_IME_CHAR;

      if (wParam == VK_F5)
        GetBrowser()->Reload();
      else
        GetBrowser()->SendKeyEvent(type, wParam, lParam, sys_char, ime_char);
    }
#endif
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

