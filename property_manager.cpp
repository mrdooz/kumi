#include "stdafx.h"
#include "property_manager.hpp"

PropertyManager *PropertyManager::_instance = nullptr;

PropertyManager& PropertyManager::instance() {
	if (!_instance)
		_instance = new PropertyManager();
	return *_instance;
}

PropertyManager::PropertyManager() {

}
