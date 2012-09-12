#pragma once

#include "../effect.hpp"
#include "../property.hpp"
#include "../camera.hpp"
#include "../gaussian_blur.hpp"
#include "../vertex_types.hpp"

struct Scene;
class DeferredContext;

class GridThing : public Effect {
public:
  GridThing(const std::string &name);
  ~GridThing();
  virtual bool init() override;
  virtual bool update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) override;
  virtual bool render() override;
  virtual bool close() override;
private:
  void addForce(int x, int y);
  void calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj);
  virtual void wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) override;

  void initGrid();
  void updateGrid(float delta);

  GraphicsObjectHandle _vb;
  GraphicsObjectHandle _ib;
  GraphicsObjectHandle _technique;

  GraphicsObjectHandle _texture;

  XMFLOAT4X4 _view, _proj;
  XMFLOAT3 _cameraPos;

  template <typename T>
  class Grid {
  public:
    Grid(int rows, int cols, bool zero) : _rows(rows), _cols(cols) { 
      _data.resize(rows*cols); 
      if (zero) 
        ZeroMemory(_data.data(), sizeof(T) * _data.size());
    }
    const T &operator()(int row, int col) const { return _data[row*_cols+col]; }
    T &operator()(int row, int col) { return _data[row*_cols+col]; }

    T *data() { return _data.data(); }
    int numCells() const { return _data.size(); }
    int dataSize() const { return numCells() * sizeof(T); }
  private:
    int _rows, _cols;
    std::vector<T> _data;
  };

  Grid<Pos4Tex> _grid;

  struct Particle {
    Particle() {}
    Particle(const XMFLOAT3 &pos, const XMFLOAT3 &vel, const XMFLOAT3 &acc) : orgPos(pos), pos(pos), vel(vel), acc(acc) {}
    XMFLOAT3 orgPos;
    XMFLOAT3 pos;
    XMFLOAT3 vel;
    XMFLOAT3 acc;
  };

  Grid<Particle> _particles;
  Grid<XMFLOAT3> _forces;

  double _curTime;

  float _stiffness, _dampening, _force;

  DeferredContext *_ctx;
};
