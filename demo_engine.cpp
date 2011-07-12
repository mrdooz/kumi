#include "stdafx.h"
#include "demo_engine.hpp"

using std::sort;

DemoEngine *DemoEngine::_instance = NULL;

DemoEngine& DemoEngine::instance() {
	if (!_instance)
		_instance = new DemoEngine;
	return *_instance;
}

DemoEngine::DemoEngine() 
	: _paused(true)
	, _frequency(0)
	, _last_time(0)
	, _current_time_ms(0)
	, _cur_effect(0)
{
}

bool DemoEngine::start() {
	_paused = false;
	QueryPerformanceCounter((LARGE_INTEGER *)&_last_time);
	return true;
}

void DemoEngine::pause(bool pause) {
	_paused = pause;
}

bool DemoEngine::init() {
	B_ERR_BOOL(!!QueryPerformanceFrequency((LARGE_INTEGER *)&_frequency));

	return true;
}

bool DemoEngine::tick() {
	if (_paused) {
		QueryPerformanceCounter((LARGE_INTEGER *)&_last_time);
		return true;
	}

	int64 now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	const int64 elapsed_ms = 1000 * (now - _last_time) / _frequency;
	_current_time_ms += elapsed_ms;

	// check for any effects that have ended
	while (!_active_effects.empty() && _active_effects.front()->_end_time <= _current_time_ms) {
		_active_effects.front()->_running = false;
		_active_effects.pop_front();
	}

	// check if any effect start now
	while (!_inactive_effects.empty() && _inactive_effects.front()->_start_time <= _current_time_ms) {
		EffectInstance *e = _inactive_effects.front();
		_inactive_effects.pop_front();
		e->_running = true;
		_active_effects.push_back(e);
	}

	// calc the number of ticks to step
	const int64 ticks_per_s = 100;
	const double tmp = (double)elapsed_ms * ticks_per_s / 1000;
	const int num_ticks = (int)tmp;
	const float frac = (float)(tmp - floor(tmp));

	// tick the active effects
	for (size_t i = 0; i < _active_effects.size(); ++i) {
		EffectInstance *e = _active_effects[i];
		e->_effect->update(_current_time_ms, _current_time_ms - e->_start_time, ticks_per_s, num_ticks, frac);
	}

	_last_time = now;

	return true;
}

bool DemoEngine::close() {
	return true;
}

void DemoEngine::add_effect(Effect *effect, uint32 start_time, uint32 end_time) {
	_effects.push_back(new EffectInstance(effect, start_time, end_time));
	sort(_effects.begin(), _effects.end(), [](const EffectInstance *a, const EffectInstance *b){ return a->_start_time < b->_start_time; });
	_active_effects.clear();
	_inactive_effects.clear();
	for (size_t i = 0; i < _effects.size(); ++i)
		_inactive_effects.push_back(_effects[i]);
}
