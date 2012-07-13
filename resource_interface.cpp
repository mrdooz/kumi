#include "stdafx.h"
#include "resource_interface.hpp"
#include "resource_manager.hpp"
#include "utils.hpp"

#if DISTRIBUTION
#else

ResourceManager *g_instance;

ResourceInterface &ResourceInterface::instance() {
  KASSERT(g_instance);
  return *g_instance;
}

bool ResourceInterface::create() {
  KASSERT(!g_instance);
  g_instance = new ResourceManager();
  return true;
}

bool ResourceInterface::close() {
  KASSERT(g_instance);
  delete exch_null(g_instance);
  return true;
}
#endif
