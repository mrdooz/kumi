#include "stdafx.h"
#include "utils.hpp"

CriticalSection::CriticalSection() 
{
	InitializeCriticalSection(&_cs);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&_cs);
}

void CriticalSection::enter()
{
	EnterCriticalSection(&_cs);
}

void CriticalSection::leave()
{
	LeaveCriticalSection(&_cs);
}


ScopedCs::ScopedCs(CriticalSection &cs) 
	: _cs(cs)
{
	_cs.enter();
}

ScopedCs::~ScopedCs()
{
	_cs.leave();
}

float gaussianRand(float mean, float variance) {
  // Generate a gaussian from the sum of uniformly distributed random numbers
  // (Central Limit Theorem)
  double sum = 0;
  const int numIters = 10;
  for (int i = 0; i < numIters; ++i) {
    sum += randf(-variance, variance);
  }
  return (float)(mean + sum / numIters);
}
