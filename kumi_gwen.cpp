#include "stdafx.h"
#include "kumi_gwen.hpp"
#include "gwen/Structures.h"
#include "gwen/Font.h"
#include "gwen/Texture.h"
#include "dynamic_vb.hpp"
#include "vertex_types.hpp"
#include "renderer.hpp"
#include "string_utils.hpp"
#include <D3DX11tex.h>

#pragma comment(lib, "D3DX11.lib")

using namespace std;

struct MappedTexture {
  D3DX11_IMAGE_INFO image_info;
  uint32 pitch;
  vector<uint8> data;
private:
};

static XMFLOAT4 to_directx(const Gwen::Color &col) {
  return XMFLOAT4(float(col.r) / 255, float(col.g) / 255, float(col.b) / 255, float(col.a) / 255);
}

static uint32 to_abgr(const XMFLOAT4 &col) {
  return ((uint32(255 * col.w)) << 24) | ((uint32(255 * col.z)) << 16) | ((uint32(255 * col.y)) << 8) | (uint32(255 * col.x));
}

struct KumiGwenRenderer : public Gwen::Renderer::Base
{
  KumiGwenRenderer()
    : _locked_pos_col(nullptr)
    , _locked_pos_tex(nullptr)
    , _cur_start_vtx(-1)
  {
    _vb_pos_col.create(64 * 1024);
    _vb_pos_tex.create(64 * 1024);
  }

  virtual void Init() {
  };

  virtual void Begin() {
    _locked_pos_col = _vb_pos_col.map();
    _locked_pos_tex = _vb_pos_tex.map();
    _cur_texture = GraphicsObjectHandle();
    _cur_start_vtx = 0;
    _textured_calls.clear();
  };

  virtual void End() {

    // draw the colored stuff
    {
      if (int pos_col_count = _vb_pos_col.unmap(exch_null(_locked_pos_col))) {
        BufferRenderData *data = RENDERER.alloc_command_data<BufferRenderData>();
        data->topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        data->vb = _vb_pos_col.vb();
        data->vertex_size = _vb_pos_col.stride;
        data->vs = GRAPHICS.find_shader("gwen_color", "gwen::vs_color_main");
        data->ps = GRAPHICS.find_shader("gwen_color", "gwen::ps_color_main");
        data->layout = GRAPHICS.get_input_layout("gwen_color");
        data->vertex_count = pos_col_count;
        GRAPHICS.get_technique_states("gwen_color", &data->rs, &data->bs, &data->dss);
        RenderKey render_key;
        render_key.cmd = RenderKey::kRenderBuffers;
        RENDERER.submit_command(FROM_HERE, render_key, data);
      }
    }

    // draw the textured stuff
    {
      int last_count = _vb_pos_tex.unmap(exch_null(_locked_pos_tex));
      if (last_count) {
        _textured_calls.push_back(TexturedRender(_cur_texture, _cur_start_vtx, last_count));
      }

      for (auto it = begin(_textured_calls); it != end(_textured_calls); ++it) {
        const TexturedRender &r = *it;
        int cnt = r.vertex_count;

        BufferRenderData *data = RENDERER.alloc_command_data<BufferRenderData>();
        data->start_vertex = r.start_vertex;
        data->vertex_count = cnt;
        data->topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        data->vb = _vb_pos_tex.vb();
        data->vertex_size = _vb_pos_tex.stride;
        data->vs = GRAPHICS.find_shader("gwen_texture", "gwen::vs_texture_main");
        data->ps = GRAPHICS.find_shader("gwen_texture", "gwen::ps_texture_main");
        data->layout = GRAPHICS.get_input_layout("gwen_texture");
        GRAPHICS.get_technique_states("gwen_texture", &data->rs, &data->bs, &data->dss);
        data->samplers[0] = GRAPHICS.get_sampler_state("gwen_texture", "diffuse_sampler");
        data->textures[0] = r.texture;
        RenderKey render_key;
        render_key.cmd = RenderKey::kRenderBuffers;
        RENDERER.submit_command(FROM_HERE, render_key, data);
      }
    }
  };

  virtual void SetDrawColor(Gwen::Color color) {
    _draw_color = to_directx(color);
  };

  virtual void DrawFilledRect(Gwen::Rect rect){
    // (0,0) ---> (w,0)
    //  |
    // (0,h)

    // 0,1
    // 2,3
    // 0, 1, 2
    // 2, 1, 3

    Translate(rect);

    float x0, y0, x1, y1, x2, y2, x3, y3;
    screen_to_clip(rect.x, rect.y, GRAPHICS.width(), GRAPHICS.height(), &x0, &y0);
    screen_to_clip(rect.x + rect.w, rect.y, GRAPHICS.width(), GRAPHICS.height(), &x1, &y1);
    screen_to_clip(rect.x, rect.y + rect.h, GRAPHICS.width(), GRAPHICS.height(), &x2, &y2);
    screen_to_clip(rect.x + rect.w, rect.y + rect.h, GRAPHICS.width(), GRAPHICS.height(), &x3, &y3);

    PosCol v0(x0, y0, 0, _draw_color);
    PosCol v1(x1, y1, 0, _draw_color);
    PosCol v2(x2, y2, 0, _draw_color);
    PosCol v3(x3, y3, 0, _draw_color);

    *_locked_pos_col++ = v0;
    *_locked_pos_col++ = v1;
    *_locked_pos_col++ = v2;

    *_locked_pos_col++ = v2;
    *_locked_pos_col++ = v1;
    *_locked_pos_col++ = v3;
  };

  virtual void StartClip() {
    const Gwen::Rect& rect = ClipRegion();
  };

  virtual void EndClip() {

  };

  virtual void LoadTexture(Gwen::Texture* pTexture) {

    D3DX11_IMAGE_INFO info;
    GraphicsObjectHandle h = GRAPHICS.load_texture(pTexture->name.c_str(), &info);
    pTexture->width = info.Width;
    pTexture->height = info.Height;
    pTexture->failed = !h.is_valid();
  };

  virtual void FreeTexture( Gwen::Texture* pTexture ){
  };

  virtual void DrawTexturedRect(Gwen::Texture* pTexture, Gwen::Rect rect, float u1=0.0f, float v1=0.0f, float u2=1.0f, float v2=1.0f) {

    Translate(rect);

    float x0, y0, x1, y1, x2, y2, x3, y3;
    screen_to_clip(rect.x, rect.y, GRAPHICS.width(), GRAPHICS.height(), &x0, &y0);
    screen_to_clip(rect.x + rect.w, rect.y, GRAPHICS.width(), GRAPHICS.height(), &x1, &y1);
    screen_to_clip(rect.x, rect.y + rect.h, GRAPHICS.width(), GRAPHICS.height(), &x2, &y2);
    screen_to_clip(rect.x + rect.w, rect.y + rect.h, GRAPHICS.width(), GRAPHICS.height(), &x3, &y3);

    GraphicsObjectHandle texture = GRAPHICS.get_texture(pTexture->name.c_str());
    if (texture != _cur_texture) {
      int cnt = 0;
      if (_cur_texture.is_valid()) {

        // add the previous texture
        _textured_calls.push_back(TexturedRender());
        TexturedRender &r = _textured_calls.back();
        r.start_vertex = _cur_start_vtx;
        cnt = _vb_pos_tex.unmap(exch_null(_locked_pos_tex));
        _locked_pos_tex = _vb_pos_tex.map();
        r.vertex_count = cnt;
        r.texture = _cur_texture;

      }
      _cur_start_vtx += cnt;
      _cur_texture = texture;
    }

    // 0,1
    // 2,3
    PosTex vv0(x0, y0, 0, u1, v1);
    PosTex vv1(x1, y1, 0, u2, v1);
    PosTex vv2(x2, y2, 0, u1, v2);
    PosTex vv3(x3, y3, 0, u2, v2);

    *_locked_pos_tex++ = vv0;
    *_locked_pos_tex++ = vv1;
    *_locked_pos_tex++ = vv2;

    *_locked_pos_tex++ = vv2;
    *_locked_pos_tex++ = vv1;
    *_locked_pos_tex++ = vv3;
  };

  virtual void DrawMissingImage( Gwen::Rect pTargetRect ) {

  }

  virtual Gwen::Color PixelColour(Gwen::Texture* pTexture, unsigned int x, unsigned int y, const Gwen::Color& col_default = Gwen::Color( 255, 255, 255, 255 ) ) { 
    auto it = _mapped_textures.find(pTexture->name);
    MappedTexture *texture = nullptr;
    if (it == _mapped_textures.end()) {
      // read the texture if it hasn't been read
      texture = &_mapped_textures[pTexture->name];
      if (!GRAPHICS.read_texture(pTexture->name.c_str(), &texture->image_info, &texture->pitch, &texture->data)) {
        _mapped_textures.erase(pTexture->name);
        return col_default;
      }
    } else {
      texture = &it->second;
    }
    uint8 *pixel = &texture->data[y*texture->pitch+x*4];
    return Gwen::Color(pixel[0], pixel[1], pixel[2], pixel[3]);
  }

  virtual Gwen::Renderer::ICacheToTexture* GetCTT() { return NULL; }

  virtual void LoadFont( Gwen::Font* pFont ){
  };

  virtual void FreeFont( Gwen::Font* pFont ){
  };

  virtual void RenderText( Gwen::Font* pFont, Gwen::Point pos, const Gwen::UnicodeString& text ) {
    Translate(pos.x, pos.y);
    RenderKey key;
    key.cmd = RenderKey::kRenderText;
    TextRenderData *data = RENDERER.alloc_command_data<TextRenderData>();
    data->font = get_font(pFont->facename);
    int len = (text.size() + 1) * sizeof(WCHAR);
    data->str = (WCHAR *)RENDERER.raw_alloc(len);
    memcpy((void *)data->str, text.c_str(), len);
    data->font_size = pFont->size;
    data->x = pos.x;
    data->y = pos.y;
    data->color = to_abgr(_draw_color);
    data->flags = 0;

    RENDERER.submit_command(FROM_HERE, key, data);
  }

  virtual Gwen::Point MeasureText(Gwen::Font* pFont, const Gwen::UnicodeString& text) {
    FW1_RECTF rect;
    GRAPHICS.measure_text(get_font(pFont->facename), pFont->facename, text, pFont->size, 0, &rect);
    return Gwen::Point(rect.Right - rect.Left, rect.Bottom - rect.Top);
  }

  GraphicsObjectHandle get_font(const std::wstring &font_name) {
    auto it = _fonts.find(font_name);
    if (it == _fonts.end()) {
      GraphicsObjectHandle font = GRAPHICS.get_or_create_font_family(font_name);
      _fonts[font_name] = font;
      return font;
    }
    return it->second;
  }

  //
  // No need to implement these functions in your derived class, but if 
  // you can do them faster than the default implementation it's a good idea to.
  //
/*
  virtual void DrawLinedRect( Gwen::Rect rect ) {
  }
  virtual void DrawPixel( int x, int y );
  virtual void DrawShavedCornerRect( Gwen::Rect rect, bool bSlight = false );
*/

  struct TexturedRender {
    TexturedRender(GraphicsObjectHandle texture, int start_vertex, int vertex_count) 
      : texture(texture), start_vertex(start_vertex), vertex_count(vertex_count) {}
    TexturedRender() 
      : start_vertex(0), vertex_count(0) {}
    GraphicsObjectHandle texture;
    int start_vertex;
    int vertex_count;
  };

  map<string, MappedTexture> _mapped_textures;
  map<wstring, GraphicsObjectHandle> _fonts;
  XMFLOAT4 _draw_color;
  DynamicVb<PosCol> _vb_pos_col;
  DynamicVb<PosTex> _vb_pos_tex;
  PosCol *_locked_pos_col;
  PosTex *_locked_pos_tex;

  int _cur_start_vtx;
  GraphicsObjectHandle _cur_texture;
  vector<TexturedRender> _textured_calls;
};


Gwen::Renderer::Base* create_kumi_gwen_renderer() {
  return new KumiGwenRenderer();
}
