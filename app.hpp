#ifndef _APP_HPP_
#define _APP_HPP_

#include "dynamic_vb.hpp"
#include "vertex_types.hpp"
#include "graphics.hpp"

using std::string;
using std::map;
using std::vector;

/*
	repeat after me: directx is left-handed. z goes into the screen.
*/

struct CTwBar;
typedef struct CTwBar TwBar;
class EffectBase;
class FontWriter;
class EffectWrapper;

struct MouseInfo
{
	MouseInfo(const bool l, const bool m, const bool r, const int x, const int y) 
		: left_down(l), middle_down(m), right_down(r), x(x), y(y), x_delta(0), y_delta(0), wheel_delta(0), time(timeGetTime()) {}
	MouseInfo(const bool l, const bool m, const bool r, const int x, const int y, const int x_delta, const int y_delta, const int wheel_delta) 
		: left_down(l), middle_down(m), right_down(r), x(x), y(y), x_delta(x_delta), y_delta(y_delta), wheel_delta(wheel_delta), time(timeGetTime()) {}
	bool	left_down;
	bool	middle_down;
	bool	right_down;

	int		x;
	int		y;
	int		x_delta, y_delta;
	int		wheel_delta;
	DWORD	time;
};

struct KeyInfo
{
	KeyInfo() : key(0), flags(0), time(timeGetTime()) {}
	KeyInfo(uint32_t key, uint32_t flags) : key(key), flags(flags), time(timeGetTime()) {}
	uint32_t key;			// copied verbatim from WM_KEY[DOWN|UP]
	uint32_t flags;
	DWORD	time;
};

typedef FastDelegate1<const MouseInfo&> fnMouseMove;
typedef FastDelegate1<const MouseInfo&> fnMouseUp;
typedef FastDelegate1<const MouseInfo&> fnMouseDown;
typedef FastDelegate1<const MouseInfo&> fnMouseWheel;

typedef FastDelegate1<const KeyInfo&> fnKeyDown;
typedef FastDelegate1<const KeyInfo&> fnKeyUp;

typedef FastDelegate4<float, float, int, float> fnUpdate;

template<class T>
class AppState
{
public:
  typedef FastDelegate2<const string&, const T&, void> fnCallback;
  typedef std::vector<fnCallback> Callbacks;

  AppState(const string& name, const T& value) : _name(name), _value(value) {}

  void add_callback(const fnCallback& fn, bool add)
  {
		if (add) {
			_callbacks.push_back(fn);
			// call the newly added callback with the default value
			fn(_name, _value);
		} else {
			auto it = std::find(_callbacks.begin(), _callbacks.end(), fn);
			if (it != _callbacks.end())
				_callbacks.erase(it);
		}
  }

  void value_changed(const T& new_value)
  {
    _value = new_value;
    for (auto i = _callbacks.begin(), e = _callbacks.end(); i != e; ++i) {
      (*i)(_name, _value);
    }
  }

	T value() const { return _value; }
private:
  string _name;
  T _value;
  Callbacks _callbacks;
};

#define ADD_APP_STATE(type, name) \
  public: \
  void add_appstate_callback(const AppState<type>::fnCallback& fn, bool add) { _state_ ## name.add_callback(fn, add); }  \
  private:  \
    AppState<type> _state_ ## name; \
  public:\

class Camera;

struct RenderStates {
	RenderStates() : blend_state(nullptr), depth_stencil_state(nullptr), rasterizer_state(nullptr), sampler_state(nullptr) {}

	D3D11_BLEND_DESC blend_desc;
	ID3D11BlendState *blend_state;

	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc;
	ID3D11DepthStencilState *depth_stencil_state;

	D3D11_RASTERIZER_DESC rasterizer_desc;
	ID3D11RasterizerState *rasterizer_state;

	D3D11_SAMPLER_DESC sampler_desc;
	ID3D11SamplerState *sampler_state;
};

struct Scene;

class App : public CefClient, public CefLifeSpanHandler, public CefLoadHandler, public CefRenderHandler
{
public:

	static App& instance();

	bool init(HINSTANCE hinstance);
	bool close();

	void	tick();

  void add_dbg_message(const char* fmt, ...);

  void run();

	void add_mouse_move(const fnMouseMove& fn, bool add);
	void add_mouse_up(const fnMouseUp& fn, bool add);
	void add_mouse_down(const fnMouseDown& fn, bool add);
	void add_mouse_wheel(const fnMouseWheel& fn, bool add);

	void add_key_down(const fnKeyDown& fn, bool add);
	void add_key_up(const fnKeyUp& fn, bool add);

  void add_update_callback(const fnUpdate& fn, bool add);

  Camera *camera();
	TwBar *tweakbar() { return _tweakbar; }

	void debug_text(const char *fmt, ...);

  ADD_APP_STATE(bool, wireframe);
private:
	DISALLOW_COPY_AND_ASSIGN(App);
	App();
	~App();

	enum MenuItem {
		kMenuQuit,
		kMenuToggleDebug,
		kMenuToggleWireframe,
    kMenuDrawXZPlane,
	};

	static void __stdcall tramp_menu(void *menu_item);

	//void resource_changed(void *token, const char *filename, const void *buf, size_t len);

	void on_quit();
	void toggle_debug();
	void toggle_wireframe();
  void toggle_plane();
	void init_menu();
  bool create_window();
  void set_client_size();
	void find_app_root();

	LRESULT wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK tramp_wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	// CefClient methods
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE { return this; }
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE { return this; }
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE { return this; }

	// CefLifeSpanHandler methods
	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser, const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo,
		const CefString& url, CefRefPtr<CefClient>& client, CefBrowserSettings& settings) OVERRIDE;
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

	// CefLoadHandler methods
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) OVERRIDE;
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) OVERRIDE;
	virtual bool OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& failedUrl, CefString& errorText) OVERRIDE;

	// CefRenderHandler methods
	virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const CefRect& dirtyRect, const void* buffer);
	virtual bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
	virtual bool GetScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
	virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX, int viewY, int& screenX, int& screenY);

	// CefJSBindingHandler
	//virtual void OnJSBinding(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Value> object);

	virtual void OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor) OVERRIDE;

	CefRefPtr<CefBrowser> GetBrowser() { return m_Browser; }
	CefWindowHandle GetBrowserHwnd() { return m_BrowserHwnd; }

	std::string GetLogFile();

	void SetLastDownloadFile(const std::string& fileName);
	std::string GetLastDownloadFile();

	// DOM visitors will be called after the associated path is loaded.
	void AddDOMVisitor(const std::string& path, CefRefPtr<CefDOMVisitor> visitor);
	CefRefPtr<CefDOMVisitor> GetDOMVisitor(const std::string& path);

	// Send a notification to the application. Notifications should not block the
	// caller.
	enum NotificationType {
		NOTIFY_CONSOLE_MESSAGE,
		NOTIFY_DOWNLOAD_COMPLETE,
		NOTIFY_DOWNLOAD_ERROR,
	};
	void SendNotification(NotificationType type);

protected:
	void SetLoading(bool isLoading);
	void SetNavState(bool canGoBack, bool canGoForward);

	// The child browser window
	CefRefPtr<CefBrowser> m_Browser;

	// The main frame window handle
	CefWindowHandle m_MainHwnd;

	// The child browser window handle
	CefWindowHandle m_BrowserHwnd;

	// The edit window handle
	CefWindowHandle m_EditHwnd;

	// The button window handles
	CefWindowHandle m_BackHwnd;
	CefWindowHandle m_ForwardHwnd;
	CefWindowHandle m_StopHwnd;
	CefWindowHandle m_ReloadHwnd;

	// Support for logging.
	std::string m_LogFile;

	// Support for downloading files.
	std::string m_LastDownloadFile;

	// Support for DOM visitors.
	typedef std::map<std::string, CefRefPtr<CefDOMVisitor> > DOMVisitorMap;
	DOMVisitorMap m_DOMVisitors;

	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(App);
	// Include the default locking implementation.
	IMPLEMENT_LOCKING(App);

	static App* _instance;
	EffectBase* _test_effect;
  HINSTANCE _hinstance;
  int32_t _width;
  int32_t _height;
  HWND _hwnd;
  int _dbg_message_count;

	CRITICAL_SECTION _cs_queue;

  std::vector< fnMouseMove > _mouse_move_callbacks;
	std::vector< fnMouseUp > _mouse_up_callbacks;
	std::vector< fnMouseDown > _mouse_down_callbacks;
	std::vector< fnMouseWheel > _mouse_wheel_callbacks;

	std::vector< fnKeyDown > _keydown_callbacks;
	std::vector< fnKeyUp > _keyup_callbacks;

  std::vector< fnUpdate > _update_callbacks;

	FontWriter *_debug_writer;
  int _cur_camera;
  Camera *_trackball;
  Camera *_freefly;
  TwBar *_tweakbar;
  bool _draw_plane;
	string _app_root;
	int _ref_count;

	ID3D11Buffer *_cef_vb;
	ID3D11InputLayout *_cef_layout;
	EffectWrapper *_cef_effect;
	RenderStates _cef_desc;
	TextureData _cef_texture;
	TextureData _cef_staging;
	Scene *_scene;
};

#define APP App::instance()

#endif
