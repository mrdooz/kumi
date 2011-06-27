#include "stdafx.h"
#include "app.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "resource_watcher.hpp"
#include "effect_wrapper.hpp"
#include "d3d_parser.hpp"
#include <boost/scoped_array.hpp>

/*
#include "system.hpp"
#include "test_effect.hpp"
#include "test_effect2.hpp"
#include "test_effect3.hpp"
#include "test_effect4.hpp"
#include "test_effect5.hpp"
#include "test_effect6.hpp"
#include "test_effect7.hpp"
#include "imgui.hpp"
#include "font_writer.hpp"
#include "debug_menu.hpp"
#include "debug_renderer.hpp"
#include <celsus/Profiler.hpp>
#include "iprof/prof.h"
#include "iprof/prof_internal.h"
#include "network.hpp"
*/

App* App::_instance = nullptr;

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	((int)(short)HIWORD(lParam))
#endif

#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")


// ClientHandler implementation.
class ClientHandler : public CefClient,
	public CefLifeSpanHandler,
	public CefLoadHandler,
	public CefJSBindingHandler,
	public CefRenderHandler
	//public DownloadListener
{
public:
	ClientHandler();
	virtual ~ClientHandler();

	// CefClient methods
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE { return this; }
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE { return this; }
	virtual CefRefPtr<CefJSBindingHandler> GetJSBindingHandler() OVERRIDE { return this; }
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE { return this; }

	// CefLifeSpanHandler methods
	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
		const CefPopupFeatures& popupFeatures,
		CefWindowInfo& windowInfo,
		const CefString& url,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& settings) OVERRIDE;
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

	// CefLoadHandler methods
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame) OVERRIDE;
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int httpStatusCode) OVERRIDE;
	virtual bool OnLoadError(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		ErrorCode errorCode,
		const CefString& failedUrl,
		CefString& errorText) OVERRIDE;

	// CefJSBindingHandler methods
	virtual void OnJSBinding(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Value> object) OVERRIDE;

	void SetMainHwnd(CefWindowHandle hwnd);
	CefWindowHandle GetMainHwnd() { return m_MainHwnd; }
	void SetEditHwnd(CefWindowHandle hwnd);
	void SetButtonHwnds(CefWindowHandle backHwnd,
		CefWindowHandle forwardHwnd,
		CefWindowHandle reloadHwnd,
		CefWindowHandle stopHwnd);

	CefRefPtr<CefBrowser> GetBrowser() { return m_Browser; }
	CefWindowHandle GetBrowserHwnd() { return m_BrowserHwnd; }

	virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const CefRect& dirtyRect, const void* buffer) {
		static int hax = 0;
		//save_bmp32(to_string("c:\\temp\\dump%.3d.bmp", hax++).c_str(), (uint8_t *)buffer, dirtyRect.width, dirtyRect.height);
		//int a = 10;
	}

	virtual bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) { 
		return false; 
	}

	virtual bool GetScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) { 
		return false; 
	}

	virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX, int viewY, int& screenX, int& screenY) { 
		return false; 
	}

	std::string GetLogFile();

	void SetLastDownloadFile(const std::string& fileName);
	std::string GetLastDownloadFile();

	// DOM visitors will be called after the associated path is loaded.
	void AddDOMVisitor(const std::string& path, CefRefPtr<CefDOMVisitor> visitor);
	CefRefPtr<CefDOMVisitor> GetDOMVisitor(const std::string& path);

	// Send a notification to the application. Notifications should not block the
	// caller.
	enum NotificationType
	{
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
	IMPLEMENT_REFCOUNTING(ClientHandler);
	// Include the default locking implementation.
	IMPLEMENT_LOCKING(ClientHandler);
};

ClientHandler::ClientHandler()
	: m_MainHwnd(NULL),
	m_BrowserHwnd(NULL),
	m_EditHwnd(NULL),
	m_BackHwnd(NULL),
	m_ForwardHwnd(NULL),
	m_StopHwnd(NULL),
	m_ReloadHwnd(NULL)
{
}

ClientHandler::~ClientHandler()
{
}

#define REQUIRE_UI_THREAD()

void ClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	REQUIRE_UI_THREAD();

	AutoLock lock_scope(this);
	if(!browser->IsPopup())
	{
		// We need to keep the main child window, but not popup windows
		m_Browser = browser;
		m_BrowserHwnd = browser->GetWindowHandle();
		browser->SetSize(PET_VIEW, 1024, 768);
	}
}

void ClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	REQUIRE_UI_THREAD();

	if(m_BrowserHwnd == browser->GetWindowHandle()) {
		// Free the browser pointer so that the browser can be destroyed
		m_Browser = NULL;
	}
}

void ClientHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame)
{
	REQUIRE_UI_THREAD();

	if(!browser->IsPopup() && frame->IsMain()) {
		// We've just started loading a page
		SetLoading(true);
	}
}

void ClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	int httpStatusCode)
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

bool ClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	ErrorCode errorCode,
	const CefString& failedUrl,
	CefString& errorText)
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

void ClientHandler::OnJSBinding(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefV8Value> object)
{
	REQUIRE_UI_THREAD();

	// Add the V8 bindings.
	//InitBindingTest(browser, frame, object);
}

void ClientHandler::SetMainHwnd(CefWindowHandle hwnd)
{
	AutoLock lock_scope(this);
	m_MainHwnd = hwnd;
}

std::string ClientHandler::GetLogFile()
{
	AutoLock lock_scope(this);
	return m_LogFile;
}

void ClientHandler::SetLastDownloadFile(const std::string& fileName)
{
	AutoLock lock_scope(this);
	m_LastDownloadFile = fileName;
}

std::string ClientHandler::GetLastDownloadFile()
{
	AutoLock lock_scope(this);
	return m_LastDownloadFile;
}

void ClientHandler::AddDOMVisitor(const std::string& path,
	CefRefPtr<CefDOMVisitor> visitor)
{
	AutoLock lock_scope(this);
	DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
	if (it == m_DOMVisitors.end())
		m_DOMVisitors.insert(std::make_pair(path, visitor));
	else
		it->second = visitor;
}

CefRefPtr<CefDOMVisitor> ClientHandler::GetDOMVisitor(const std::string& path)
{
	AutoLock lock_scope(this);
	DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
	if (it != m_DOMVisitors.end())
		return it->second;
	return NULL;
}

bool ClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
	const CefPopupFeatures& popupFeatures,
	CefWindowInfo& windowInfo,
	const CefString& url,
	CefRefPtr<CefClient>& client,
	CefBrowserSettings& settings)
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


void ClientHandler::SetLoading(bool isLoading)
{
	//ASSERT(m_EditHwnd != NULL && m_ReloadHwnd != NULL && m_StopHwnd != NULL);
	EnableWindow(m_EditHwnd, TRUE);
	EnableWindow(m_ReloadHwnd, !isLoading);
	EnableWindow(m_StopHwnd, isLoading);
}

void ClientHandler::SetNavState(bool canGoBack, bool canGoForward)
{
	//ASSERT(m_BackHwnd != NULL && m_ForwardHwnd != NULL);
	EnableWindow(m_BackHwnd, canGoBack);
	EnableWindow(m_ForwardHwnd, canGoForward);
}


CefRefPtr<ClientHandler> g_handler;

void create_input_desc(const PosTex *verts, InputDesc *desc) {
	desc->
		add("SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0).
		add("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0);
}

template<typename T>
bool vb_from_data(const T *verts, int num_verts, ID3D11Buffer **vb, InputDesc *desc)
{
	B_WRN_DX(create_static_vertex_buffer(GRAPHICS.device(), num_verts, sizeof(T), vertex, vb));
	create_input_desc(verts, desc);
	return true;
}

struct StateDesc {
	StateDesc() : blend_desc(NULL), depth_stencil_desc(NULL), rasterizer_desc(NULL), sampler_desc(NULL) {}
	D3D11_BLEND_DESC *blend_desc;
	D3D11_DEPTH_STENCIL_DESC *depth_stencil_desc;
	D3D11_RASTERIZER_DESC *rasterizer_desc;
	D3D11_SAMPLER_DESC *sampler_desc;
};

template<typename T>
bool simple_load(const T *verts, int num_verts, 
	const char *shader, const char *vs, const char *gs, const char *ps, 
	const char *state, const char *blend_desc, const char *depth_stencil_desc, const char *rasterizer_desc, const char *sampler_desc,
	ID3D11Buffer **vb, ID3D11InputLayout **layout, EffectWrapper **effect, StateDesc *state_desc) 
{
	InputDesc desc;
	B_WRN_BOOL(vb_from_data(verts, num_verts, vb, &desc));

	EffectWrapper *e = new EffectWrapper;
	B_WRN_BOOL(e->load_shaders(shader, vs, gs, ps));

	if (state && state_desc) {
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
			rasterizer_desc ? rasterizer_descs : NULL,
			sampler_desc ? sampler_descs : NULL);

#define TRY_GET(n) if (n) state_desc->n = n # s[n];
		TRY_GET(blend_desc);
		TRY_GET(depth_stencil_desc);
		TRY_GET(rasterizer_desc);
		TRY_GET(sampler_desc);
#undef TRY_GET
	}

	*layout = desc.create(e);

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

//byte font_buf[512*512];
//stbtt_bakedchar chars[96];
//int texture_height;

#include "text_parser.hpp"
void parse_text(const char *str, vector<Token> *out);

struct TokenIterator {
	typedef vector<Token> Tokens;
	TokenIterator(const Tokens &tokens) : tokens(tokens), ofs(0) {}
	const Token *next()
	{
		if (ofs >= tokens.size())
			return NULL;
		return &tokens[ofs++];
	}

	void rewind(int num)
	{
		ofs = max(0, ofs - num);
	}

	const vector<Token> &tokens;
	size_t ofs;
};

class FontManager {
public:
	enum Style {
		kNormal,
		kBold,
		kItalic
	};

	static FontManager &instance();

	bool init();
	void close();

	bool create_font_class(const char *family, Style weight, Style style, const char *filename);

	bool create_font_class(const char *family, int size, Style weight, Style style, const char *filename);
	void add_text(const char *fmt, ...);
	void render();
private:
	FontManager();
	~FontManager();

	struct FontClass;
	struct FontInstance {
		FontInstance(const FontClass *fc, int size, int w, int h, const stbtt_bakedchar *baked, const TextureData &texture)
			: font_class(fc), size(size), w(w), h(h), texture(texture) { memcpy(chars, baked, sizeof(chars)); }
		const FontClass *font_class;
		int size;
		int w, h;
		stbtt_bakedchar chars[96];
		TextureData texture;
	};

	struct FontClass {
		FontClass(const string &family, Style weight, Style style) : family(family), weight(weight), style(style), buf(0), len(0) {}
		FontClass(const string &filename, const string &family, Style weight, Style style, void *buf, size_t len) 
			: filename(filename), family(family), weight(weight), style(style), buf(buf), len(len) {}
		~FontClass() 
		{
			delete [] exch_null(buf);
			container_delete(instances);
		}
		friend bool operator==(const FontClass &a, const FontClass &b)
		{
			return a.family == b.family && a.weight == b.weight && a.style == b.style;
		}
		string filename;
		string family;
		Style weight;
		Style style;
		void *buf;
		size_t len;
		vector<FontInstance *> instances;
	};

	bool create_font_instance(const FontClass *fc, int font_size, FontInstance **fi);

	void resource_changed(void *token, const char *filename, const void *buf, size_t len);

	bool find_font(const string &family, int size, Style weight, Style style, FontInstance **fi);

	vector<PosTex> _vb_data;
	DynamicVb<PosTex> _font_vb;
	EffectWrapper *_debug_fx;
	CComPtr<ID3D11InputLayout> _text_layout;
	CComPtr<ID3D11RasterizerState> _rasterizer_state;
	CComPtr<ID3D11DepthStencilState> _dss_state;
	CComPtr<ID3D11SamplerState> _sampler_state;
	CComPtr<ID3D11BlendState> _blend_state;
	struct DrawCall {
		DrawCall() {}
		DrawCall(int ofs, int num) : ofs(ofs), num(num) {}
		int ofs, num;
	};
	typedef map<ID3D11ShaderResourceView *, vector<DrawCall> > DrawCalls;
	DrawCalls _draw_calls;

	vector<FontClass *> _font_classes;

	static FontManager *_instance;
};

FontManager *FontManager::_instance = NULL;

FontManager::FontManager()
	: _debug_fx(nullptr)
{

}

FontManager::~FontManager()
{
	delete exch_null(_debug_fx);
}

const char *debug_font = "effects/debug_font.fx";
const char *debug_font_state = "effects/debug_font_states.txt";

bool FontManager::init()
{
	B_ERR_BOOL(_font_vb.create(16 * 1024));
	RESOURCE_WATCHER.add_file_watch(NULL, debug_font, true, MakeDelegate(this, &FontManager::resource_changed));
	RESOURCE_WATCHER.add_file_watch(NULL, debug_font_state, true, MakeDelegate(this, &FontManager::resource_changed));

	return true;
}

void FontManager::close()
{
	delete exch_null(_instance);
}


void FontManager::resource_changed(void *token, const char *filename, const void *buf, size_t len)
{
	if (!strcmp(filename, debug_font)) {
		EffectWrapper *tmp = new EffectWrapper;
		if (tmp->load_shaders((const char *)buf, len, "vsMain", NULL, "psMain")) {
			delete exch_null(_debug_fx);
			_debug_fx = tmp;
			_text_layout.Attach(InputDesc(). 
				add("SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0).
				add("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0).
				create(_debug_fx));
		}
	} else if (!strcmp(filename, debug_font_state)) {

		map<string, D3D11_RASTERIZER_DESC> r;
		map<string, D3D11_SAMPLER_DESC> s;
		map<string, D3D11_BLEND_DESC> b;
		map<string, D3D11_DEPTH_STENCIL_DESC> d;
		parse_descs((const char *)buf, (const char *)buf + len, &b, &d, &r, &s);
		LOG_WRN_DX(GRAPHICS.device()->CreateSamplerState(&s["debug_font"], &_sampler_state.p));
		LOG_WRN_DX(GRAPHICS.device()->CreateBlendState(&b["bs"], &_blend_state.p));
	}

	GRAPHICS.device()->CreateSamplerState(&CD3D11_SAMPLER_DESC(CD3D11_DEFAULT()), &_sampler_state.p);
}


FontManager &FontManager::instance()
{
	if (!_instance)
		_instance = new FontManager;
	return *_instance;
}

#define FONT_MANAGER FontManager::instance()

void FontManager::add_text(const char *fmt, ...)
{
	// valid properties are: [[ pos-x: XX | pos-y: XX | font-family: XXX | font-size: XX | font-style: [Normal|Italic] | font-weight: [Normal|Bold] ]]+
	const D3D11_VIEWPORT &vp = GRAPHICS.viewport();

	//const char *str = "[[pos-y: 200]]magnus mcagnus[[ font-size: 10 ]]lilla magne[[ font-size: 100 ]]stora magne!";
	const char *str = "[[pos-y: 200]]magnus";
	//const char *str = "[[pos-y: 20]]gx";
	vector<Token> tokens;
	parse_text(str, &tokens);

	string font_family = "Arial";
	FontManager::Style font_style = FontManager::kNormal;
	FontManager::Style font_weight = FontManager::kNormal;
	int font_size = 50;
	FontInstance *fi;
	if (!find_font(font_family, font_size, font_weight, font_style, &fi))
		return;

	bool new_font = false;

	bool cmd_mode = false;
	float x = 0, y = 0;
	TokenIterator it(tokens);
	const Token *token = NULL;
	const Token *next;
	bool parse_error = false;

	int ofs = 0, num = 0;

	while (!parse_error && (token = it.next())) {

		switch (token->type) {

		case Token::kChar:
			{
				if (new_font) {
					// Changing fonts, so save # primitives to draw with the old one
					_draw_calls[fi->texture.srv].push_back(DrawCall(ofs, _vb_data.size() - ofs));
					ofs = _vb_data.size();
				}
				if (!new_font || new_font && find_font(font_family, font_size, font_weight, font_style, &fi)) {
					new_font = false;
					char ch = token->ch;
					stbtt_aligned_quad q;
					stbtt_GetBakedQuad(fi->chars, fi->w, fi->h, ch - 32, &x, &y, &q, 1);
					// v0 v1
					// v2 v3
					PosTex v0, v1, v2, v3;
					screen_to_clip(q.x0, q.y0, vp.Width, vp.Height, &v0.pos.x, &v0.pos.y); v0.pos.z = 0; v0.tex.x = q.s0; v0.tex.y = q.t0;
					screen_to_clip(q.x1, q.y0, vp.Width, vp.Height, &v1.pos.x, &v1.pos.y); v1.pos.z = 0; v1.tex.x = q.s1; v1.tex.y = q.t0;
					screen_to_clip(q.x0, q.y1, vp.Width, vp.Height, &v2.pos.x, &v2.pos.y); v2.pos.z = 0; v2.tex.x = q.s0; v2.tex.y = q.t1;
					screen_to_clip(q.x1, q.y1, vp.Width, vp.Height, &v3.pos.x, &v3.pos.y); v3.pos.z = 0; v3.tex.x = q.s1; v3.tex.y = q.t1;
					_vb_data.push_back(v0); _vb_data.push_back(v1); _vb_data.push_back(v2);
					_vb_data.push_back(v2); _vb_data.push_back(v1); _vb_data.push_back(v3);
				}
			}
			break;

		case Token::kCmdOpen:
			if (cmd_mode) {
				parse_error = true;
				continue;
			}
			cmd_mode = true;
			break;

		case Token::kCmdClose:
			cmd_mode = false;
			break;
#define CHECK_NEXT(type) if (!(next = it.next()) || next->type != type) { parse_error = true; continue; }

		case Token::kPosX:
			CHECK_NEXT(Token::kInt);
			x = (float)next->num;
			break;

		case Token::kPosY:
			CHECK_NEXT(Token::kInt);
			y = (float)next->num;
			break;

		case Token::kFontSize:
			CHECK_NEXT(Token::kInt);
			if (next->num != font_size) {
				font_size = next->num;
				new_font = true;
			}
			break;

		case Token::kFontStyle:
			CHECK_NEXT(Token::kId);
			if (!strcmp(token->id, "normal")) {
				new_font |= font_style != FontManager::kNormal;
				font_style = FontManager::kNormal;
			} else if (!strcmp(token->id, "italic")) {
				new_font |= font_style != FontManager::kItalic;
				font_style = FontManager::kItalic;
			} else {
				parse_error = true;
			}
			break;

		case Token::kFontWeight:
			CHECK_NEXT(Token::kId);
			if (!strcmp(token->id, "normal")) {
				new_font |= font_weight != FontManager::kNormal;
				font_style = FontManager::kNormal;
			}
			else if (!strcmp(token->id, "bold")) {
				new_font |= font_weight != FontManager::kBold;
				font_style = FontManager::kBold;
			} else {
				parse_error = true;
			}
			break;

		case Token::kFontFamily:
			CHECK_NEXT(Token::kId);
			new_font |= !strcmp(font_family.c_str(), next->id);
			font_family = next->id;
			break;
		default:
			parse_error = true;
		}
	}

	if (ofs != _vb_data.size())
		_draw_calls[fi->texture.srv].push_back(DrawCall(ofs, _vb_data.size() - ofs));
}

void FontManager::render()
{
	if (_vb_data.empty())
		return;

	PosTex *p = _font_vb.map();
	memcpy(p, &_vb_data[0], _vb_data.size() * sizeof(PosTex));
	_vb_data.clear();
	_font_vb.unmap(p);

	ID3D11DeviceContext *context = GRAPHICS.context();
	context->RSSetViewports(1, &GRAPHICS.viewport());

	context->PSSetSamplers(0, 1, &_sampler_state.p);

	_debug_fx->set_shaders(context);
	set_vb(context, _font_vb.get(), _font_vb.stride);
	context->IASetInputLayout(_text_layout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	float blend_factor[] = { 1, 1, 1, 1 };
	context->OMSetBlendState(GRAPHICS.default_blend_state(), blend_factor, 0xffffffff);
	context->RSSetState(GRAPHICS.default_rasterizer_state());
	context->OMSetDepthStencilState(_dss_state, 0xffffffff);
	context->PSSetSamplers(0, 1, &_sampler_state.p);
	//context->OMSetBlendState(_blend_state, GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());
	//context->Draw(_font_vb.num_verts(), 0);
	for (auto i = _draw_calls.begin(); i != _draw_calls.end(); ++i) {
		ID3D11ShaderResourceView *srv = i->first;
		context->PSSetShaderResources(0, 1, &srv);
		for (auto j = i->second.begin(); j != i->second.end(); ++j) {
			const DrawCall &cur = *j;
			context->Draw(cur.num, cur.ofs);
		}
	}
	_draw_calls.clear();
}

bool FontManager::find_font(const string &family, int size, Style weight, Style style, FontInstance **fi)
{
	// look for the FontClass
	FontClass fc(family, weight, style);
	for (size_t i = 0; i < _font_classes.size(); ++i) {
		FontClass *cur = _font_classes[i];
		if (fc == *cur) {
			// look for instance
			for (size_t j = 0; j < cur->instances.size(); ++j) {
				if (cur->instances[j]->size == size) {
					*fi = cur->instances[j];
					return true;
				}
			}
			// instance of the specified size wasn't found, so we create it
			FontInstance *f;
			if (!create_font_instance(cur, size, &f))
				return false;
			cur->instances.push_back(f);
			*fi = f;
			return true;
		}
	}
	return false;
}

bool FontManager::create_font_class(const char *family, Style weight, Style style, const char *filename)
{
	void *buf;
	size_t len;
	B_ERR_BOOL(load_file(filename, &buf, &len));
	_font_classes.push_back(new FontClass(filename, family, weight, style, buf, len));
	return true;
}

bool FontManager::create_font_instance(const FontClass *fc, int font_size, FontInstance **fi)
{
	const int num_chars = 96;
	const int w = num_chars * font_size;
	int mult = 1;
	// expand the buffer until all the chars fit
	while (true) {
		int h = mult * font_size;
		boost::scoped_array<uint8_t> font_buf(new uint8_t[w*h]);
		stbtt_bakedchar chars[96];
		int res = stbtt_BakeFontBitmap((const byte *)fc->buf, 0, (float)font_size, font_buf.get(), w, h, 32, num_chars, chars);
		if (res < 0) {
			mult++;
		} else {
			TextureData texture;
			//static int hax = 0;
			//save_bmp_mono(to_string("c:\\temp\\texture_%d.bmp", hax++).c_str(), font_buf.get(), w, res);
			B_ERR_BOOL(GRAPHICS.create_texture(w, res, DXGI_FORMAT_A8_UNORM, font_buf.get(), w, res, w, &texture));
			*fi = new FontInstance(fc, font_size, w, h, chars, texture);
			return true;
		}
	}
	return false;
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
	B_ERR_BOOL(GRAPHICS.init(_hwnd, _width, _height));

	PosTex verts[] = {
		// 0 1
		// 2 3
		PosTex(0, 0, 0, 0, 0),
		PosTex(1, 0, 0, 1, 0),
		PosTex(0, 1, 0, 0, 1),
		PosTex(1, 1, 0, 1, 1),
	};

	ID3D11Buffer *vb = NULL;
	ID3D11InputLayout *layout = NULL;
	EffectWrapper *effect = NULL;
	StateDesc desc;
	simple_load(verts, ELEMS_IN_ARRAY(verts), 
		"effects/debug_font.fx", "vsMain", NULL, "psMain", 
		"effects/debug_font_states.txt", "", "", "mr_tjong", "debug_font", 

	FONT_MANAGER.init();
	//FONT_MANAGER.create_font_class("Arial", FontManager::kNormal, FontManager::kNormal, "data/arialbd.ttf");

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
	//auto& graphics = Graphics::instance();

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

			FONT_MANAGER.add_text("tjong!");
			FONT_MANAGER.render();
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

LRESULT App::wnd_proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{

	HDC hdc;
	PAINTSTRUCT ps;
	switch( message ) 
	{
/*
	case WM_SETFOCUS:
		if(g_handler.get() && g_handler->GetBrowserHwnd()) {
			// Pass focus to the browser window
			PostMessage(g_handler->GetBrowserHwnd(), WM_SETFOCUS, wParam, NULL);
		}
		return 0;
*/
	case WM_SIZE:
		GRAPHICS.resize(LOWORD(lParam), HIWORD(lParam));
		if(g_handler.get() && g_handler->GetBrowserHwnd()) {
			// Resize the browser window and address bar to match the new frame
			// window size
			RECT rect;
			GetClientRect(hWnd, &rect);
			HDWP hdwp = BeginDeferWindowPos(1);
			hdwp = DeferWindowPos(hdwp, g_handler->GetBrowserHwnd(), 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
			EndDeferWindowPos(hdwp);
			g_handler->GetBrowser()->SetSize(PET_VIEW, rect.right - rect.left, rect.bottom - rect.top);
		}
		break;
/*
	case WM_ERASEBKGND:
		if(g_handler.get() && g_handler->GetBrowserHwnd()) {
			// Dont erase the background if the browser window has been loaded
			// (this avoids flashing)
			return 0;
		}
		break;
*/

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_CREATE:
		{
			g_handler = new ClientHandler();
			g_handler->SetMainHwnd(hWnd);

			CefWindowInfo info;
			info.m_bIsTransparent = TRUE;
			CefBrowserSettings settings;

			RECT rect;
			GetClientRect(hWnd, &rect);
			info.SetAsOffScreen(hWnd);
			info.SetIsTransparent(TRUE);
			CefBrowser::CreateBrowser(info, static_cast<CefRefPtr<CefClient> >(g_handler), "file://C:/Users/dooz/Downloads/PlasterDemo/PlasterDemo/Assets/index.html", settings);
			//g_handler->GetBrowser()->SetSize(PET_VIEW, 1024, 768);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
/*
		ui_state.mouse_down = 1;
		{
			MouseInfo m(!!(wParam & MK_LBUTTON), !!(wParam & MK_MBUTTON), !!(wParam & MK_RBUTTON), LOWORD(lParam), HIWORD(lParam));
			for (auto i = _mouse_down_callbacks.begin(), e = _mouse_down_callbacks.end(); i != e; ++i)
				(*i)(m);
		}
*/
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
/*
		ui_state.mouse_down = 0;
		{
			MouseInfo m(!!(wParam & MK_LBUTTON), !!(wParam & MK_MBUTTON), !!(wParam & MK_RBUTTON), LOWORD(lParam), HIWORD(lParam));
			for (auto i = _mouse_up_callbacks.begin(), e = _mouse_up_callbacks.end(); i != e; ++i)
				(*i)(m);
		}
*/
		break;

	case WM_MOUSEMOVE:
/*
		ui_state.mouse_x = GET_X_LPARAM(lParam);
		ui_state.mouse_y = GET_Y_LPARAM(lParam);
		{
			// only call the callbacks if we've actually moved
			static int last_wparam = 0;
			static int last_lparam = 0;
			if (last_lparam != lParam || last_wparam != wParam) {
				int x = LOWORD(lParam);
				int y = HIWORD(lParam);
				MouseInfo m(!!(wParam & MK_LBUTTON), !!(wParam & MK_MBUTTON), !!(wParam & MK_RBUTTON), x, y, x - LOWORD(last_lparam), y - HIWORD(last_lparam), 0);
				for (auto i = _mouse_move_callbacks.begin(), e = _mouse_move_callbacks.end(); i != e; ++i)
					(*i)(m);
				last_lparam = lParam;
				last_wparam = wParam;
			}
		}
*/
		break;

	case WM_MOUSEWHEEL:
/*
		{
			MouseInfo m(!!(wParam & MK_LBUTTON), !!(wParam & MK_MBUTTON), !!(wParam & MK_RBUTTON), LOWORD(lParam), HIWORD(lParam), 0, 0, GET_WHEEL_DELTA_WPARAM(wParam));
			for (auto i = _mouse_wheel_callbacks.begin(), e = _mouse_wheel_callbacks.end(); i != e; ++i)
				(*i)(m);
		}
*/
		break;

	case WM_KEYDOWN:
/*
		{
			KeyInfo k(wParam, lParam);
			for (auto i = _keydown_callbacks.begin(), e = _keydown_callbacks.end(); i != e; ++i)
				(*i)(k);
		}
*/
		break;

	case WM_CHAR:
		//ui_state.key_entered = wParam;
		break;

	case WM_KEYUP:
		switch (wParam) 
		{
		case VK_ESCAPE:
			PostQuitMessage( 0 );
			break;
		default:
/*
			{
				KeyInfo k(wParam, lParam);
				for (auto i = _keyup_callbacks.begin(), e = _keyup_callbacks.end(); i != e; ++i)
					(*i)(k);
			}
*/
			break;
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

