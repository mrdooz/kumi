#pragma once

class FreeflyCamera {
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

  FreeflyCamera();

  void move(Direction dir, float amount);
  void rotate(Axis axis, float amount);

  XMFLOAT4X4 viewMatrix();
  XMFLOAT4X4 projectionMatrix();

  void setPos(const XMFLOAT3 &pos);
  void setPos(float x, float y, float z);

  const XMFLOAT3 &pos() const { return _pos; }
  const XMFLOAT3 &dir() const { return _dir; }
  const XMFLOAT3 &right() const { return _right; }
  const XMFLOAT3 &up() const { return _up; }

private:
  float _xAngle, _yAngle;
  float _fov, _aspectRatio;
  float _zNear, _zFar;

  XMFLOAT3 _pos;
  XMFLOAT3 _dir, _right, _up;
};
