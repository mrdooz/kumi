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

