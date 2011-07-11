#include "stdafx.h"
#include "demo_engine.hpp"

DemoEngine *DemoEngine::_instance = NULL;

DemoEngine& DemoEngine::instance() {
	if (!_instance)
		_instance = new DemoEngine;
	return *_instance;
}

DemoEngine::DemoEngine() 
	: _paused(true)
{
	QueryPerformanceFrequency(&_frequency);
}

bool DemoEngine::start() {
	_paused = false;
	return true;
}

void DemoEngine::pause(bool pause) {
	_paused = pause;
}

bool DemoEngine::init() {
	return true;
}

bool DemoEngine::tick() {
	if (_paused)
		return true;

	return true;
}

bool DemoEngine::close() {
	return true;
}

void DemoEngine::add_effect(Effect *effect, uint32 start_time, uint32 end_time) {
	_effects.push(EffectInstance(effect, start_time, end_time));
}


