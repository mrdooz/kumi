#include "stdafx.h"
#include "font_manager.hpp"
#include "effect_wrapper.hpp"
#include "resource_watcher.hpp"
#include "d3d_parser.hpp"
#include "text_parser.hpp"
#include "file_utils.hpp"

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
