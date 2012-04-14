#pragma once
#include <cassert>
#include "dx_utils.hpp"
#include "graphics.hpp"
#include "tracked_location.hpp"

// Helper class for dynamic vertex buffers
template<class Vtx>
class DynamicVb
{
public:

  enum { stride = sizeof(Vtx) };
  DynamicVb()
    : _mapped(false)
    , _org(nullptr)
    , _num_verts(0)
  {
  }

  bool create(int max_verts) {
    return (_vb = GRAPHICS.create_dynamic_vertex_buffer(FROM_HERE, max_verts * sizeof(Vtx))).is_valid();
  }

  Vtx *map() {
    D3D11_MAPPED_SUBRESOURCE r;
    if (!GRAPHICS.map(_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &r))
      return NULL;

    _mapped = true;
    _org = (Vtx*)r.pData;
    return _org;
  }

  int unmap(Vtx *final = NULL)
  {
    assert(_mapped);
    if (!_mapped) return -1;
    GRAPHICS.unmap(_vb, 0);
    _mapped = false;

    // calc # verts inserted
    return (_num_verts = final != NULL ? final - _org : 0);
  }

  ID3D11Buffer *get() { return _vb; }
  int num_verts() const { return _num_verts; }
  GraphicsObjectHandle vb() const { return _vb; }

  bool is_mapped() const { return _mapped; }

private:
  Vtx *_org;
  bool _mapped;
  int _num_verts;
  GraphicsObjectHandle _vb;
};



// Helper class for appendable dynamic vertex buffers
template<class Vtx>
class AppendableVb
{
public:

  enum { stride = sizeof(Vtx) };
  AppendableVb()
    : _mapped(false)
    , _org(nullptr)
    , _ofs(0)
  {
  }

  ~AppendableVb()
  {
  }

  bool create(int max_verts)
  {
    return SUCCEEDED(create_dynamic_vertex_buffer(Graphics::instance().device(), max_verts, sizeof(Vtx), &_vb));
  }

  Vtx *map()
  {
    ID3D11DeviceContext *c = Graphics::instance().context();
    D3D11_MAPPED_SUBRESOURCE r;
    // first map is done with discard
    if (FAILED(c->Map(_vb, 0, _ofs == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &r)))
      return NULL;

    _mapped = true;
    _org = (Vtx*)r.pData;
    return _org + _ofs;
  }

  int unmap(Vtx *final)
  {
    assert(_mapped);
    if (!_mapped) return -1;
    ID3D11DeviceContext *c = Graphics::instance().context();
    c->Unmap(_vb, 0);
    _mapped = false;
    _ofs = final - _org;
    return _ofs;
  }

  void reset()
  {
    assert(!_mapped);
    _ofs = 0;
    _num_verts = 0;
  }

  ID3D11Buffer *get() { return _vb; }
  int num_verts() const { return _ofs; }

private:
  Vtx *_org;
  bool _mapped;
  int _ofs;
  CComPtr<ID3D11Buffer> _vb;
};

