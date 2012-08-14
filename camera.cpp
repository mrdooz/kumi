#include "stdafx.h"
#include "camera.hpp"

FreeFlyCamera::FreeFlyCamera() 
  : theta(XM_PIDIV2)
  , rho(XM_PIDIV2)
  , roll(0)
  , pos(0,0,0)
  , dir(0,0,1)
  , right(1,0,0)
  , up(0,1,0) 
{
  // points down the z-axis
} 
