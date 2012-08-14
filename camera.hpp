#pragma once

struct FreeFlyCamera {
  FreeFlyCamera();
  float theta;
  float rho;
  float roll;
  XMFLOAT3 pos;
  XMFLOAT3 dir, right, up;
};
