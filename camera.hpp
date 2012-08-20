#pragma once

class FreeFlyCamera {
public:
  enum Direction {
    kRight,
    kUp,
    kForward,
  };

  enum Axis {
    kXAxis,
    kYAxis,
  };

  FreeFlyCamera();

  void move(Direction dir, float amount);
  void rotate(Axis axis, float amount);

  XMFLOAT4X4 viewMatrix();

  const XMFLOAT3 &pos() const { return _pos; }
  const XMFLOAT3 &dir() const { return _dir; }
  const XMFLOAT3 &right() const { return _right; }
  const XMFLOAT3 &up() const { return _up; }

private:
  float _xAngle, _yAngle;
  XMFLOAT3 _pos;
  XMFLOAT3 _dir, _right, _up;
};
