#pragma once

#include "graphics.hpp"
#include "vertex_types.hpp"
#include "dynamic_vb.hpp"

using std::vector;

class EffectWrapper;
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

#define FONT_MANAGER FontManager::instance()
