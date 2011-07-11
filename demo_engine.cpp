#include "stdafx.h"
#include "demo_engine.hpp"

DemoEngine *DemoEngine::_instance = NULL;

DemoEngine& DemoEngine::instance() {
	if (!_instance)
		_instance = new DemoEngine;
	return *_instance;
}

DemoEngine::DemoEngine() {
}

bool DemoEngine::init() {
	return true;
}

bool DemoEngine::tick() {
	return true;
}

bool DemoEngine::close() {
	return true;
}

