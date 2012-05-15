#include "stdafx.h"
#include "property_manager.hpp"
#include "utils.hpp"

PropertyManager *PropertyManager::_instance = nullptr;

PropertyManager& PropertyManager::instance() {
  assert(_instance);
	return *_instance;
}

bool PropertyManager::create() {
  assert(!_instance);
  _instance = new PropertyManager;
  return true;
}

bool PropertyManager::close() {
  assert(_instance);
  delete exch_null(_instance);
  return true;
}


PropertyManager::PropertyManager() 
  : _property_ofs(0)
{
  _raw_data.resize(1 << 20);
}
