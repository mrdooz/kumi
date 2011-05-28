#include "stdafx.h"
#include "app.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "resource_watcher.hpp"
#include "effect_wrapper.hpp"
#include "d3d_parser.hpp"

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
	, _debug_fx(nullptr)
{
	find_app_root();
}

App::~App()
{
	delete exch_null(_debug_fx);
}

App& App::instance()
{
	if (_instance == NULL)
		_instance = new App();
	return *_instance;
}

byte font_buf[512*512];
stbtt_bakedchar chars[96];
int texture_height;

void App::debug_text(const char *fmt, ...)
{
	const D3D11_VIEWPORT &vp = GRAPHICS.viewport();

	const char *str = "magnus mcagnus";
	float x = 200;
	float y = 200;
	PosTex *p = _font_vb.map();
	PosTex v0, v1, v2, v3;
	for (int i = 0, e = strlen(str); i < e; ++i) {
		char ch = str[i];
		stbtt_aligned_quad q;
		stbtt_GetBakedQuad(chars, 512, texture_height, ch - 32, &x, &y, &q, 1);
		// v0 v1
		// v2 v3
		screen_to_clip(q.x0, q.y0, vp.Width, vp.Height, &v0.pos.x, &v0.pos.y); v0.pos.z = 0; v0.tex.x = q.s0; v0.tex.y = q.t0;
		screen_to_clip(q.x1, q.y0, vp.Width, vp.Height, &v1.pos.x, &v1.pos.y); v1.pos.z = 0; v1.tex.x = q.s1; v1.tex.y = q.t0;
		screen_to_clip(q.x0, q.y1, vp.Width, vp.Height, &v2.pos.x, &v2.pos.y); v2.pos.z = 0; v2.tex.x = q.s0; v2.tex.y = q.t1;
		screen_to_clip(q.x1, q.y1, vp.Width, vp.Height, &v3.pos.x, &v3.pos.y); v3.pos.z = 0; v3.tex.x = q.s1; v3.tex.y = q.t1;
		*p++ = v0; *p++ = v1; *p++ = v2;
		*p++ = v2; *p++ = v1; *p++ = v3;
	}
	const int num_verts = _font_vb.unmap(p);

}

const char *debug_font = "effects/debug_font.fx";
const char *debug_font_state = "effects/debug_font_states.txt";

void App::resource_changed(void *token, const char *filename, const void *buf, size_t len)
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
		parse_descs((const char *)buf, (const char *)buf + len, NULL, NULL, &r, &s);
		GRAPHICS.device()->CreateSamplerState(&s["debug_font"], &_sampler_state.p);
/*
		D3D11_RASTERIZER_DESC desc;
		string name;
		if (parse_rasterizer_desc((const char *)buf, (const char *)buf + len, &desc, &name)) {
			ID3D11RasterizerState *state;
			GRAPHICS.device()->CreateRasterizerState(&desc, &state);
			_rasterizer_state.Attach(state);
		}
*/
/*
		D3D11_DEPTH_STENCIL_DESC desc;
		if (parse_depth_stencil_desc((const char *)buf, (const char *)buf + len, &desc)) {
			ID3D11DepthStencilState *state;
			GRAPHICS.device()->CreateDepthStencilState(&desc, &state);
			_dss_state.Attach(state);
		}
*/
	}

	GRAPHICS.device()->CreateSamplerState(&CD3D11_SAMPLER_DESC(CD3D11_DEFAULT()), &_sampler_state.p);
}

struct FontManager {
	enum Style {
		kNormal,
		kBold,
		kItalic
	};

	void add_font(const char *family, Style style, const char *filename);
	void render(int x, int y, const char *str, ...);
};

void FontManager::add_font(const char *family, Style style, const char *filename)
{

}

void FontManager::render(int x, int y, const char *str, ...)
{
	// valid properties are: { font-family: XXX }, { font-size: XX }, { font-style: [Normal|Italic] }, { font-width: [Bold|Italic] }

}

bool App::init(HINSTANCE hinstance)
{
	_hinstance = hinstance;
	_width = GetSystemMetrics(SM_CXSCREEN) / 2;
	_height = GetSystemMetrics(SM_CYSCREEN) / 2;
	
	B_ERR_BOOL(create_window());
	B_ERR_BOOL(GRAPHICS.init(_hwnd, _width, _height));

	void *font;
	size_t len;
	B_ERR_BOOL(load_file("data/arialbd.ttf", &font, &len));
	int res = stbtt_BakeFontBitmap((const byte *)font, 0, 50, font_buf, 512, 512, 32, 96, chars);
	const int width = 512;
	const int height = res > 0 ? res : 512;
	texture_height = height;
	//save_bmp_mono("c:\\temp\\tjoff.bmp", font_buf, width, height);

	B_ERR_BOOL(GRAPHICS.create_texture(512, res, DXGI_FORMAT_A8_UNORM, font_buf, width, height, width, &_texture));
	B_ERR_BOOL(_font_vb.create(16 * 1024));

	RESOURCE_WATCHER.add_file_watch(NULL, debug_font, true, MakeDelegate(this, &App::resource_changed));
	RESOURCE_WATCHER.add_file_watch(NULL, debug_font_state, true, MakeDelegate(this, &App::resource_changed));

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
	const char* kClassName = "ShindigClass";

	WNDCLASSEXA wcex;
	ZeroMemory(&wcex, sizeof(wcex));

	wcex.cbSize = sizeof(WNDCLASSEXA);
	wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wcex.lpfnWndProc    = tramp_wnd_proc;
	wcex.hInstance      = _hinstance;
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszClassName  = kClassName;

	B_ERR_INT(RegisterClassExA(&wcex));

	//const UINT window_style = WS_VISIBLE | WS_POPUP | WS_OVERLAPPEDWINDOW;
	const UINT window_style = WS_VISIBLE | WS_POPUP;

	_hwnd = CreateWindowA(kClassName, "kumi - magnus österlind - 2011", window_style,
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
			RESOURCE_WATCHER.process_deferred();
			GRAPHICS.set_clear_color(XMFLOAT4(0.3f, 0.3f, 0.3f, 1));
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


			ID3D11DeviceContext *context = GRAPHICS.context();
			context->RSSetViewports(1, &GRAPHICS.viewport());

			context->PSSetShaderResources(0, 1, &_texture.srv.p);
			context->PSSetSamplers(0, 1, &_sampler_state.p);

			_debug_fx->set_shaders(context);

			debug_text("tjong");
			set_vb(context, _font_vb.get(), _font_vb.stride);
			context->IASetInputLayout(_text_layout);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			float blend_factor[] = { 1, 1, 1, 1 };
			context->OMSetBlendState(GRAPHICS.default_blend_state(), blend_factor, 0xffffffff);
			context->RSSetState(GRAPHICS.default_rasterizer_state());
			context->OMSetDepthStencilState(_dss_state, 0xffffffff);
			context->PSSetSamplers(0, 1, &_sampler_state.p);

			context->Draw(_font_vb.num_verts(), 0);

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
	// See if the debug menu wants to handle the message first
	//LRESULT res = DebugMenu::instance().wnd_proc(hWnd, message, wParam, lParam);

	//UIState& ui_state = IMGui::instance().ui_state();

	switch( message ) 
	{
	case WM_SIZE:
		GRAPHICS.resize(LOWORD(lParam), HIWORD(lParam));
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

