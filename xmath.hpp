#pragma once

// simple math routines using the XMFLOAT-family of structures
// It feels like overkill to use the store/get functions when I'm just doing single vector calculations..

inline XMFLOAT4X4 transpose(const XMFLOAT4X4 &mtx) {
  return XMFLOAT4X4(
    mtx._11, mtx._21, mtx._31, mtx._41,
    mtx._12, mtx._22, mtx._32, mtx._42,
    mtx._13, mtx._23, mtx._33, mtx._43,
    mtx._14, mtx._24, mtx._34, mtx._44);
}

inline XMFLOAT4X4 perspective_foh(float fov_y, float aspect, float zn, float zf) {
  float y_scale = (float)(1 / tan(fov_y/2));
  float x_scale = y_scale / aspect;
  return XMFLOAT4X4(
    x_scale,    0,          0,              0,
    0,          y_scale,    0,              0,
    0,          0,          zf/(zf-zn),     1,
    0,          0,          -zn*zf/(zf-zn), 0
    );
}

inline XMFLOAT3 operator*(float s, const XMFLOAT3 &rhs) {
  return XMFLOAT3(s*rhs.x, s*rhs.y, s*rhs.z);
}

inline XMFLOAT3 &operator+=(XMFLOAT3 &lhs, const XMFLOAT3 &rhs) {
  lhs.x += rhs.x;
  lhs.y += rhs.y;
  lhs.z += rhs.z;
  return lhs;
}

inline XMFLOAT3 &operator-=(XMFLOAT3 &lhs, const XMFLOAT3 &rhs) {
  lhs.x -= rhs.x;
  lhs.y -= rhs.y;
  lhs.z -= rhs.z;
  return lhs;
}

inline XMFLOAT4X4 mtx_identity() {
  return XMFLOAT4X4(
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1);
}

inline XMFLOAT3 vec3_from_spherical(float theta, float rho) {
  // theta is angle around the x-axis
  // rho is angle around the y-axis
  float st = sinf(theta);
  float sr = sinf(rho);
  float ct = cosf(theta);
  float cr = cosf(rho);
  return XMFLOAT3(st*cr, ct, st*sr);
}

inline XMFLOAT4 row(const XMFLOAT4X4 &m, int row) {
  return XMFLOAT4(m.m[row][0], m.m[row][1], m.m[row][2], m.m[row][3]);
}

inline void set_row(const XMFLOAT3 &v, int row, XMFLOAT4X4 *mtx) {
  mtx->m[row][0] = v.x;
  mtx->m[row][1] = v.y;
  mtx->m[row][2] = v.z;
}

inline void set_col(const XMFLOAT3 &v, int col, XMFLOAT4X4 *mtx) {
  mtx->m[0][col] = v.x;
  mtx->m[1][col] = v.y;
  mtx->m[2][col] = v.z;
}

inline XMFLOAT4 col(const XMFLOAT4X4 &m, int col) {
  return XMFLOAT4(m.m[0][col], m.m[1][col], m.m[2][col], m.m[3][col]);
}

inline float dot(const XMFLOAT4 &a, const XMFLOAT4 &b) {
  return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

inline float dot(const XMFLOAT3 &a, const XMFLOAT3 &b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline float len(const XMFLOAT3 &a) {
  return sqrtf(dot(a,a));
}

inline XMFLOAT3 normalize(const XMFLOAT3 &a) {
  return 1/len(a) * a;
}

inline XMFLOAT3 cross(const XMFLOAT3 &a, const XMFLOAT3 &b) {
  return XMFLOAT3(
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
    );
}

inline XMFLOAT4 expand(const XMFLOAT3 &a, float w) {
  return XMFLOAT4(a.x, a.y, a.z, w);
}

inline XMFLOAT3 project(const XMFLOAT4 &a) {
  return XMFLOAT3(a.x/a.w, a.y/a.w, a.z/a.w);
}

inline XMFLOAT3 drop(const XMFLOAT4 &a) {
  return XMFLOAT3(a.x, a.y, a.z);
}


inline XMFLOAT4 operator*(const XMFLOAT4 &v, const XMFLOAT4X4 &m) {
  return XMFLOAT4(
    dot(v, col(m, 0)),
    dot(v, col(m, 1)),
    dot(v, col(m, 2)),
    dot(v, col(m, 3)));
}

inline XMFLOAT4X4 mtx_from_axis_angle(const XMFLOAT3 &v, float angle) {

  XMFLOAT4X4 res(mtx_identity());
  res.m[0][0] = (1.0f - cos(angle)) * v.x * v.x + cos(angle);
  res.m[1][0] = (1.0f - cos(angle)) * v.x * v.y - sin(angle) * v.z;
  res.m[2][0] = (1.0f - cos(angle)) * v.x * v.z + sin(angle) * v.y;
  res.m[0][1] = (1.0f - cos(angle)) * v.y * v.x + sin(angle) * v.z;
  res.m[1][1] = (1.0f - cos(angle)) * v.y * v.y + cos(angle);
  res.m[2][1] = (1.0f - cos(angle)) * v.y * v.z - sin(angle) * v.x;
  res.m[0][2] = (1.0f - cos(angle)) * v.z * v.x - sin(angle) * v.y;
  res.m[1][2] = (1.0f - cos(angle)) * v.z * v.y + sin(angle) * v.x;
  res.m[2][2] = (1.0f - cos(angle)) * v.z * v.z + cos(angle);

  return res;
}
