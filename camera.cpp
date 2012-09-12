#include "stdafx.h"
#include "camera.hpp"

FreeflyCamera::FreeflyCamera() 
  : _xAngle(0)
  , _yAngle(0)
  , _pos(0,0,0)
  , _dir(0,0,1)
  , _right(1,0,0)
  , _up(0,1,0)
  , _fov(45 * XM_PI / 180)
  , _aspectRatio(1.6f)
  , _zNear(0.1f)
  , _zFar(2500)
{
  // points down the z-axis
} 

void FreeflyCamera::move(Direction dir, float amount) {

  switch (dir) {
    case FreeflyCamera::kRight:
      _pos += amount * _right;
      break;
    case FreeflyCamera::kUp:
      _pos += amount * _up;
      break;
    case FreeflyCamera::kForward:
      _pos += amount * _dir;
      break;
    default:
      KASSERT(false);
      break;
  }
}

void FreeflyCamera::rotate(Axis axis, float amount) {

  switch (axis) {
    case FreeflyCamera::kXAxis:
      _xAngle += amount;
      break;
    case FreeflyCamera::kYAxis:
      _yAngle += amount;
      break;
    default:
      KASSERT(false);
      break;
  }
}

XMFLOAT4X4 FreeflyCamera::projectionMatrix() {
  return perspective_foh(_fov, _aspectRatio, _zNear, _zFar);
}

XMFLOAT4X4 FreeflyCamera::viewMatrix() {

  XMMATRIX a = XMLoadFloat4x4(&mtx_from_axis_angle(XMFLOAT3(0,1,0), _xAngle));
  XMMATRIX b = XMLoadFloat4x4(&mtx_from_axis_angle(XMFLOAT3(1,0,0), _yAngle));
  XMMATRIX c = XMMatrixMultiply(b, a);

  XMFLOAT4X4 d;
  XMStoreFloat4x4(&d, c);
  _right = drop(XMFLOAT4(1,0,0,0) * d);
  _up = drop(XMFLOAT4(0,1,0,0) * d);
  _dir = drop(XMFLOAT4(0,0,1,0) * d);

  // Camera mtx = inverse of camera -> world mtx, so we construct the inverse inplace
  // R^-1         0
  // -R^-1 * t    1

  XMFLOAT4X4 view(mtx_identity());
  set_col(_right, 0, &view);
  set_col(_up, 1, &view);
  set_col(_dir, 2, &view);

  view._41 = -dot(_right, _pos);
  view._42 = -dot(_up, _pos);
  view._43 = -dot(_dir, _pos);

  return view;
}

void FreeflyCamera::setPos(const XMFLOAT3 &pos) {
  _pos = pos;
}

void FreeflyCamera::setPos(float x, float y, float z) {
  _pos.x = x;
  _pos.y = y;
  _pos.z = z;
}
