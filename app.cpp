#include "stdafx.h"
#include "app.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "resource_watcher.hpp"
#include "effect_wrapper.hpp"
#include "d3d_parser.hpp"
#include "file_utils.hpp"
#include "string_utils.hpp"

App* App::_instance = nullptr;

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	((int)(short)HIWORD(lParam))
#endif

#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")

#define REQUIRE_UI_THREAD()

void App::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const CefRect& dirtyRect, const void* buffer) {
	D3D11_MAPPED_SUBRESOURCE sub;
	GRAPHICS.context()->Map(_cef_staging.texture, 0, D3D11_MAP_WRITE, 0, &sub);
	uint8_t *dst = (uint8_t *)sub.pData + 4 * dirtyRect.x + dirtyRect.y * sub.RowPitch;
	uint8_t *src = (uint8_t *)buffer + 4 * (dirtyRect.x + dirtyRect.y * _width);
	const int src_pitch = _width * 4;
	for (int i = 0; i < dirtyRect.height; ++i) {
		memcpy(dst, src, src_pitch);
		dst += sub.RowPitch;
		src += src_pitch;
	}
	GRAPHICS.context()->Unmap(_cef_staging.texture, 0);

	GRAPHICS.context()->CopyResource(_cef_texture.texture, _cef_staging.texture);
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
		m_BrowserHwnd = browser->GetWindowHandle();
		browser->SetSize(PET_VIEW, _width, _height);
	}
}

void App::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	REQUIRE_UI_THREAD();

	if(m_BrowserHwnd == browser->GetWindowHandle()) {
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

bool App::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& failedUrl, CefString& errorText)
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

void App::SetMainHwnd(CefWindowHandle hwnd)
{
	AutoLock lock_scope(this);
	m_MainHwnd = hwnd;
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

bool App::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser, const CefPopupFeatures& popupFeatures, CefWindowInfo& windowInfo, 
	const CefString& url, CefRefPtr<CefClient>& client, CefBrowserSettings& settings)
{
	REQUIRE_UI_THREAD();

#ifdef TEST_REDIRECT_POPUP_URLS
	std::string urlStr = url;
	if(urlStr.find("resources/inspector/devtools.html") == std::string::npos) {
		// Show all popup windows excluding DevTools in the current window.
		windowInfo.m_dwStyle &= ~WS_VISIBLE;
		client = new ClientPopupHandler(m_Browser);
	}
#endif // TEST_REDIRECT_POPUP_URLS

	return false;
}


void App::SetLoading(bool isLoading)
{
	//ASSERT(m_EditHwnd != NULL && m_ReloadHwnd != NULL && m_StopHwnd != NULL);
	EnableWindow(m_EditHwnd, TRUE);
	EnableWindow(m_ReloadHwnd, !isLoading);
	EnableWindow(m_StopHwnd, isLoading);
}

void App::SetNavState(bool canGoBack, bool canGoForward)
{
	//ASSERT(m_BackHwnd != NULL && m_ForwardHwnd != NULL);
	EnableWindow(m_BackHwnd, canGoBack);
	EnableWindow(m_ForwardHwnd, canGoForward);
}

/*
void App::OnJSBinding(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Value> object)
{
	REQUIRE_UI_THREAD();

	// Create the new V8 object.
	CefRefPtr<CefV8Value> testObjPtr = CefV8Value::CreateObject(NULL);

	// Add the new V8 object to the global window object with the name
	// "cef_test".
	object->SetValue("cef_test", testObjPtr);

	// Create an instance of ClientV8FunctionHandler as the V8 handler.
	CefRefPtr<CefV8Handler> handlerPtr = new MyV8Handler();

	// Add a new V8 function to the cef_test object with the name "Dump".
	testObjPtr->SetValue("Dump", CefV8Value::CreateFunction("Dump", handlerPtr));
	// Add a new V8 function to the cef_test object with the name "Call".
	testObjPtr->SetValue("Call", CefV8Value::CreateFunction("Call", handlerPtr));


	// Add the V8 bindings.
	//InitBindingTest(browser, frame, object);
}
*/

//CefRefPtr<ClientHandler> g_handler;

void create_input_desc(const PosTex *verts, InputDesc *desc) 
{
	desc->
		add("SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0).
		add("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0);
}

template<typename T>
bool vb_from_data(const T *verts, int num_verts, ID3D11Buffer **vb, InputDesc *desc)
{
	B_WRN_DX(GRAPHICS.create_static_vertex_buffer(num_verts, sizeof(T), verts, vb));
	create_input_desc(verts, desc);
	return true;
}


template<typename T>
bool simple_load(const T *verts, int num_verts,
	const char *shader, const char *vs, const char *gs, const char *ps, 
	const char *state, const char *blend_desc, const char *depth_stencil_desc, const char *rasterizer_desc, const char *sampler_desc,
	ID3D11Buffer **vb, ID3D11InputLayout **layout, EffectWrapper **effect, RenderStates *render_states) 
{
	InputDesc desc;
	B_WRN_BOOL(vb_from_data(verts, num_verts, vb, &desc));

	*effect = EffectWrapper::create_from_file(shader, vs, gs, ps);
	B_WRN_BOOL(!!(*effect));

	if (state && render_states) {
		void *buf = NULL;
		size_t len = 0;
		B_WRN_BOOL(load_file(state, &buf, &len));
		map<string, D3D11_BLEND_DESC> blend_descs;
		map<string, D3D11_DEPTH_STENCIL_DESC> depth_stencil_descs;
		map<string, D3D11_RASTERIZER_DESC> rasterizer_descs;
		map<string, D3D11_SAMPLER_DESC> sampler_descs;
		parse_descs((const char *)buf, (const char *)buf + len, 
			blend_desc ? &blend_descs : NULL,
			depth_stencil_desc ? &depth_stencil_descs : NULL,
			rasterizer_desc ? &rasterizer_descs : NULL,
			sampler_desc ? &sampler_descs : NULL);

#define CREATE_STATE(n, fn) \
	if (n ## _desc && n ## _descs.find(n ## _desc) != n ## _descs.end()) { \
			render_states->n ## _desc = n ## _descs[n ## _desc];	\
			GRAPHICS.device()->fn(&render_states->n ## _desc, &render_states->n ## _state); \
	}

		CREATE_STATE(blend, CreateBlendState);
		CREATE_STATE(depth_stencil, CreateDepthStencilState);
		CREATE_STATE(rasterizer, CreateRasterizerState);
		CREATE_STATE(sampler, CreateSamplerState);

#undef CREATE_STATE
	}

	*layout = desc.create(*effect);

	return true;
}


App::App()
	: _hinstance(NULL)
	, _width(-1)
	, _height(-1)
	, _hwnd(NULL)
	, _test_effect(NULL)
	, _debug_writer(nullptr)
	, _state_wireframe("wireframe", false)
	, _dbg_message_count(0)
	, _trackball(nullptr)
	, _freefly(nullptr)
	, _cur_camera(1)
	, _draw_plane(false)
	, _ref_count(1)
	, _cef_vb(nullptr)
	, _cef_layout(nullptr)
	, _cef_effect(nullptr)
	, m_MainHwnd(NULL),
	m_BrowserHwnd(NULL),
	m_EditHwnd(NULL),
	m_BackHwnd(NULL),
	m_ForwardHwnd(NULL),
	m_StopHwnd(NULL),
	m_ReloadHwnd(NULL)
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
	_hinstance = hinstance;
	_width = GetSystemMetrics(SM_CXSCREEN) / 2;
	_height = GetSystemMetrics(SM_CYSCREEN) / 2;
	
	CefSettings settings;
	settings.multi_threaded_message_loop = false;

	if (!CefInitialize(settings))
		return false;

	B_ERR_BOOL(create_window());

	PosTex verts[6];
	for (int i = 0; i < 6; ++i)
		verts[i].pos.z = 0;

	const int stride = sizeof(PosTex);
	make_quad_clip_space_unindexed(&verts[0].pos.x, &verts[0].pos.y, &verts[0].tex.x, &verts[0].tex.y,
		stride, stride, stride, stride, 0, 0, 1, 1);

	B_ERR_BOOL(simple_load(verts, ELEMS_IN_ARRAY(verts), 
		"effects/debug_font.fx", "vsMain", NULL, "psMain", 
		"effects/debug_font_states.txt", "blend", "", "mr_tjong", "debug_font", 
		&_cef_vb, &_cef_layout, &_cef_effect, &_cef_desc));
/*
	GRAPHICS.create_texture(
		CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, _width, _height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE),
		&_cef_texture);
*/
	return true;
}

void App::on_quit()
{
	SendMessage(_hwnd, WM_DESTROY, 0, 0);
}

void App::init_menu()
{
	//DebugMenu::instance().add_button("quit", fastdelegate::MakeDelegate(this, &App::on_quit));
	//DebugMenu::instance().add_button("quit2", fastdelegate::MakeDelegate(this, &App::on_quit));
}

bool App::close()
{
	B_ERR_BOOL(GRAPHICS.close());
	delete this;
	_instance = nullptr;

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
	const TCHAR* kClassName = _T("KumiClass");

	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = tramp_wnd_proc;
	wcex.hInstance      = _hinstance;
	wcex.hbrBackground  = 0;
	wcex.lpszClassName  = kClassName;

	B_ERR_INT(RegisterClassEx(&wcex));

	//const UINT window_style = WS_VISIBLE | WS_POPUP | WS_OVERLAPPEDWINDOW;
	const UINT window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS; //WS_VISIBLE | WS_POPUP;

	_hwnd = CreateWindow(kClassName, _T("kumi - magnus österlind - 2011"), window_style,
		CW_USEDEFAULT, CW_USEDEFAULT, _width, _height, NULL, NULL,
		_hinstance, NULL);

	set_client_size();

	ShowWindow(_hwnd, SW_SHOW);

	return true;
}

void App::tick()
{
}

void App::tramp_menu(void *menu_item)
{
	switch ((MenuItem)(int)menu_item) {
	case kMenuQuit:
		App::instance().on_quit();
		break;
	case kMenuToggleDebug:
		App::instance().toggle_debug();
		break;
	case kMenuToggleWireframe:
		App::instance().toggle_wireframe();
		break;
	case kMenuDrawXZPlane:
		App::instance().toggle_plane();
	}
}

void App::toggle_debug()
{
	//DebugRenderer::instance().set_enabled(!DebugRenderer::instance().enabled());
}

void App::toggle_wireframe()
{
	//_state_wireframe.value_changed(!_state_wireframe.value());
}

void App::toggle_plane()
{
	//_draw_plane = !_draw_plane;
}

void App::run()
{
	MSG msg = {0};

	float running_time = 0;
	const float dt = 1 / 100.0f;

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	LARGE_INTEGER cur;
	QueryPerformanceCounter(&cur);
	float cur_time = (float)(cur.QuadPart / freq.QuadPart);
	float accumulator = 0;

	while (WM_QUIT != msg.message) {
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {

			CefDoMessageLoopWork();

			RESOURCE_WATCHER.process_deferred();
			static int hax = 0;
			GRAPHICS.set_clear_color(XMFLOAT4(0.5f + 0.5f * sin(hax++/1000.0f), 0.3f, 0.3f, 0));
			GRAPHICS.clear();
			//System::instance().tick();
			//graphics.clear();
			//_debug_writer->reset_frame();
			//IMGui::instance().init_frame();

			//DebugRenderer::instance().start_frame();

			{
				GRAPHICS.context()->CopyResource(_cef_texture.texture, _cef_staging.texture);
				ID3D11DeviceContext *context = GRAPHICS.context();
				context->PSSetSamplers(0, 1, &_cef_desc.sampler_state);
				_cef_effect->set_shaders(context);
				_cef_effect->set_resource("g_texture", _cef_texture.srv);
				GRAPHICS.set_vb(context, _cef_vb, sizeof(PosTex));
				context->IASetInputLayout(_cef_layout);
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				float blend_factor[] = { 1, 1, 1, 1 };
				context->OMSetBlendState(_cef_desc.blend_state, blend_factor, 0xffffffff);
				context->RSSetState(_cef_desc.rasterizer_state);
				context->OMSetDepthStencilState(_cef_desc.depth_stencil_state, 0xffffffff);
				context->Draw(6, 0);
			}

			if (_test_effect) {

				QueryPerformanceCounter(&cur);
				float new_time = (float)cur.QuadPart / freq.QuadPart;
				float delta_time = new_time - cur_time;
				cur_time = new_time;
				accumulator += delta_time;

				// calc the number of ticks to step
				int num_ticks = (int)(accumulator / dt);
				const float a = delta_time > 0 ? (accumulator - num_ticks * dt) / delta_time : 0;

				for (int i = 0; i < (int)_update_callbacks.size(); ++i)
					_update_callbacks[i](running_time, dt, num_ticks, a);

				running_time += num_ticks * dt;
				accumulator -= num_ticks * dt;

				{
					//Prof_Zone(render);
					//_test_effect->render();

				}
			}

			GRAPHICS.present();
		}
	}
}

LRESULT App::tramp_wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//if( TwEventWin(hWnd, message, wParam, lParam) ) // send event message to AntTweakBar
		//return 0;

	return App::instance().wnd_proc(hWnd, message, wParam, lParam);
}

int funky(int a, float b, CefString c) {
	return 42;
}

CefString monkey(CefString c) {
	return CefString("apa");
}

namespace function_types = boost::function_types;
namespace mpl = boost::mpl;

typedef bool (CefV8Value::*CheckTypeFn)();
typedef vector<CheckTypeFn> Validators;

struct MakeValidatorList {
	MakeValidatorList(Validators &v) : _validators(v) {}
	Validators &_validators;

	template<typename T> struct add_validator;
	template<> struct add_validator<int> { void add(Validators &v) { v.push_back(&CefV8Value::IsInt); } };
	template<> struct add_validator<bool> { void add(Validators &v) { v.push_back(&CefV8Value::IsBool); } };
	template<> struct add_validator<float> { void add(Validators &v) { v.push_back(&CefV8Value::IsDouble); } };
	template<> struct add_validator<double> { void add(Validators &v) { v.push_back(&CefV8Value::IsDouble); } };
	template<> struct add_validator<string> { void add(Validators &v) { v.push_back(&CefV8Value::IsString); } };
	template<> struct add_validator<CefString> { void add(Validators &v) { v.push_back(&CefV8Value::IsString); } };

	template <typename T>
	void operator()(T t) {
		add_validator<T> v;
		v.add(_validators);
	}
};

// Transform that removes the references from types in the sequence
struct remove_ref {
	template <class T>
	struct apply {
		typedef typename boost::remove_reference<T>::type type;
	};
};

struct MyV8Handler : public CefV8Handler {

	struct Func {
		Func(const string &cef_name, const string &native_name, const string &args) : _cef_name(cef_name), _native_name(native_name), _args(args) {}
		virtual bool is_valid(const CefV8ValueList& arguments) = 0;
		virtual CefRefPtr<CefV8Value> execute(const CefV8ValueList& arguments) = 0;
		string _cef_name;
		string _native_name;
		string _args;
		Validators _validators;
	};

	template<int T> struct Int2Type {}; 

	template<class Fn>
	struct FuncT : public Func {

		// typedefs for the parameters and return type
		typedef typename mpl::transform< function_types::parameter_types<Fn>, remove_ref>::type ParamTypes;
		typedef typename function_types::result_type<Fn>::type ResultType;

		FuncT(const string &cef_name, const string &native_name, const string &args, const Fn &fn) : Func(cef_name, native_name, args), _fn(fn) {}

		virtual bool is_valid(const CefV8ValueList& arguments) {
			if (arguments.size() != _validators.size())
				return false;
			// Apply the validator for each argument to check it's of the correct type
			for (size_t i = 0; i < _validators.size(); ++i) {
				if (!((arguments[i].get())->*_validators[i])())
					return false;
			}
			return true;
		}

		// unboxes a CefV8Value to a native type
		template<typename T> struct unbox_value;
		template<> struct unbox_value<bool> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator bool() { return _v.GetBoolValue(); } };
		template<> struct unbox_value<int> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator int() { return _v.GetIntValue(); } };
		template<> struct unbox_value<float> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator float() { return (float)_v.GetDoubleValue(); } };
		template<> struct unbox_value<double> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator double() { return _v.GetDoubleValue(); } };
		template<> struct unbox_value<CefString> { unbox_value(CefV8Value& v) : _v(v) {} CefV8Value& _v; operator CefString() { return _v.GetStringValue(); } };

		// box a native type in a CefV8Value
		template<typename T> struct box_value;
		template<> struct box_value<bool> { box_value(bool v) : _v(v) {} bool _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateBool(_v); } };
		template<> struct box_value<int> { box_value(int v) : _v(v) {} int _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateInt(_v); } };
		template<> struct box_value<float> { box_value(float v) : _v(v) {} float _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateDouble(_v); } };
		template<> struct box_value<double> { box_value(double v) : _v(v) {} double _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateDouble(_v); } };
		template<> struct box_value<CefString> { box_value(CefString v) : _v(v) {} CefString _v; operator CefRefPtr<CefV8Value>() { return CefV8Value::CreateString(_v); } };

		// specialize the executors on the function arity
		template<class Arity, class ParamTypes, class ResultType> struct exec;

		template<class ParamTypes, class ResultType> struct exec< Int2Type<0>, ParamTypes, ResultType> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				fn();
				return CefV8Value::CreateNull();
			}
		};

		// for each argument, get its type, and call the correct unbox_value-specialization to unbox the value
		template<class ParamTypes, class ResultType> struct exec< Int2Type<1>, ParamTypes, ResultType> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get())));
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<2>, ParamTypes, ResultType> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()), 
					unbox_value<mpl::at<ParamTypes, mpl::int_<1> >::type>(*arguments[1].get())));
			}
		};

		template<class ParamTypes, class ResultType> struct exec< Int2Type<3>, ParamTypes, ResultType> {
			template<class Fn> CefRefPtr<CefV8Value> operator()(Fn& fn, const CefV8ValueList& arguments) {
				return box_value<ResultType>(fn(
					unbox_value<mpl::at<ParamTypes, mpl::int_<0> >::type>(*arguments[0].get()), 
					unbox_value<mpl::at<ParamTypes, mpl::int_<1> >::type>(*arguments[1].get()), 
					unbox_value<mpl::at<ParamTypes, mpl::int_<2> >::type>(*arguments[2].get())));
			}
		};

		virtual CefRefPtr<CefV8Value> execute(const CefV8ValueList& arguments) {
			typedef exec< Int2Type<function_types::function_arity<Fn>::value>, ParamTypes, ResultType> E;
			E e;
			return e(_fn, arguments);
		}

		Fn _fn;
	};

	virtual bool Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception)
	{
		Funcs::iterator it = _funcs.find(name);
		if (it == _funcs.end())
			return false;

		Func *func = it->second;
		if (!func->is_valid(arguments))
			return false;

		retval = func->execute(arguments);

		return true;
	}

	template<typename Arity> struct get_param_string;
	template<> struct get_param_string<Int2Type<0>> { operator string() { return ""; } };
	template<> struct get_param_string<Int2Type<1>> { operator string() { return "a"; } };
	template<> struct get_param_string<Int2Type<2>> { operator string() { return "a,b"; } };
	template<> struct get_param_string<Int2Type<3>> { operator string() { return "a,b,c"; } };

	template <typename Fn> 
	void AddFunction(const char *cef_name, const char *native_name, Fn fn) {
		assert(_funcs.find(cef_name) == _funcs.end());
		Func *f = new FuncT<Fn>(cef_name, native_name, get_param_string<Int2Type<function_types::function_arity<Fn>::value>>(), fn);
		_funcs[cef_name] = f;

		// create a validator for the function
		mpl::for_each<mpl::transform< function_types::parameter_types<Fn>, remove_ref>::type>(MakeValidatorList(f->_validators));
	}

	void Register()
	{
		string code = "var cef;"
			"if (!cef)\n"
			"  cef = {};\n"
			"if (!cef.test)\n"
			"  cef.test = {};\n";

		const char *fn_template = ""
			"cef.test.%s = function(%s) {\n"
			"  native function %s(%s);\n"
			"  return %s(%s);\n"
			"};\n";

		// use the template to create the function bindings
		for (auto it = _funcs.begin(); it != _funcs.end(); ++it) {
			const Func *cur = it->second;
			code += to_string(fn_template, 
				cur->_cef_name.c_str(), cur->_args.c_str(), cur->_native_name.c_str(), cur->_args.c_str(), cur->_native_name.c_str(), cur->_args.c_str());
		}

		CefRegisterExtension("v8/test", code, this);
	}

	string _code;

	typedef map<string, Func *> Funcs;
	Funcs _funcs;

	IMPLEMENT_REFCOUNTING(MyV8Handler);
};

void InitExtensionTest()
{
	MyV8Handler *vv = new MyV8Handler;
	vv->AddFunction("funky", "funky", funky);
	vv->AddFunction("funky2", "funky", funky);
	vv->AddFunction("monkey", "monkey", monkey);
	vv->Register();
}

LRESULT App::wnd_proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{

	//Validators vv;
	//make_funky(funky, vv);

	HDC hdc;
	PAINTSTRUCT ps;
	switch( message ) 
	{

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		{
			SetFocus(_hwnd);
			if (GetBrowser())
				GetBrowser()->SetFocus(message == WM_SETFOCUS);
		}
		return 0;

	case WM_SIZE:
		GRAPHICS.resize(LOWORD(lParam), HIWORD(lParam));
		GRAPHICS.create_texture(
			CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, _width, _height, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE),
			&_cef_texture);

		GRAPHICS.create_texture(
			CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, _width, _height, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_WRITE),
			&_cef_staging);

		if (GetBrowserHwnd()) {
			RECT rect;
			GetClientRect(hWnd, &rect);
			HDWP hdwp = BeginDeferWindowPos(1);
			hdwp = DeferWindowPos(hdwp, GetBrowserHwnd(), 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
			EndDeferWindowPos(hdwp);
			GetBrowser()->SetSize(PET_VIEW, rect.right - rect.left, rect.bottom - rect.top);
		}
		break;

	case WM_ERASEBKGND:
		return 0;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		return 0;

	case WM_CREATE:
		{
			_hwnd = hWnd;
			B_ERR_BOOL(GRAPHICS.init(_hwnd, _width, _height));

			SetMainHwnd(hWnd);

			CefWindowInfo info;
			info.m_bIsTransparent = TRUE;
			CefBrowserSettings settings;

			RECT rect;
			GetClientRect(hWnd, &rect);
			info.SetAsOffScreen(hWnd);
			info.SetIsTransparent(TRUE);
			CefBrowser::CreateBrowser(info, static_cast<CefRefPtr<CefClient> >(this), "file://C:/temp/tjong.html", settings);
			InitExtensionTest();
			//CefBrowser::CreateBrowser(info, static_cast<CefRefPtr<CefClient> >(this), "http://www.google.com", settings);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		if (GetBrowser()) {
			if (GetCapture() == hWnd)
				ReleaseCapture();
			cef_mouse_button_type_t btn = message == WM_LBUTTONUP ? MBT_LEFT : (message == WM_MBUTTONUP ? MBT_MIDDLE : MBT_RIGHT);
			GetBrowser()->SendMouseClickEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), btn, true, 1);
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if (GetBrowser()) {
			SetCapture(hWnd);
			SetFocus(hWnd);
			cef_mouse_button_type_t btn = message == WM_LBUTTONDOWN ? MBT_LEFT : (message == WM_MBUTTONDOWN ? MBT_MIDDLE : MBT_RIGHT);
			GetBrowser()->SendMouseClickEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), btn, false, 1);
		}
		break;

	case WM_MOUSEMOVE:
		if (GetBrowser())
			GetBrowser()->SendMouseMoveEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
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

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_IME_CHAR:
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
		break;

	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}


void App::add_mouse_move(const fnMouseMove& fn, bool add)
{
	if (add) {
		_mouse_move_callbacks.push_back(fn);
	} else {
		safe_erase(_mouse_move_callbacks, fn);
	}
}

void App::add_mouse_up(const fnMouseUp& fn, bool add)
{
	if (add) {
		_mouse_up_callbacks.push_back(fn);
	} else {
		safe_erase(_mouse_up_callbacks, fn);
	}
}

void App::add_mouse_down(const fnMouseDown& fn, bool add)
{
	if (add) {
		_mouse_down_callbacks.push_back(fn);
	} else {
		safe_erase(_mouse_down_callbacks, fn);
	}
}

void App::add_mouse_wheel(const fnMouseWheel& fn, bool add)
{
	if (add) {
		_mouse_wheel_callbacks.push_back(fn);
	} else {
		safe_erase(_mouse_wheel_callbacks, fn);
	}
}

void App::add_key_down(const fnKeyDown& fn, bool add)
{
	if (add) {
		_keydown_callbacks.push_back(fn);
	} else {
		safe_erase(_keydown_callbacks, fn);
	}
}

void App::add_key_up(const fnKeyUp& fn, bool add)
{
	if (add) {
		_keyup_callbacks.push_back(fn);
	} else {
		safe_erase(_keyup_callbacks, fn);
	}
}

void App::add_update_callback(const fnUpdate& fn, bool add)
{
	if (add)
		_update_callbacks.push_back(fn);
	else
		safe_erase(_update_callbacks, fn);
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


Camera *App::camera()
{
	switch (_cur_camera) {
		case 0 : return _trackball;
		case 1 : return _freefly;
	}
	return NULL;
}

