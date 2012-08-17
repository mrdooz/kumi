#include "stdafx.h"
#include "resource_interface.hpp"
#include "resource_manager.hpp"
#include "utils.hpp"

ResourceManager *g_instance;

ResourceInterface &ResourceInterface::instance() {
  KASSERT(g_instance);
  return *g_instance;
}

bool ResourceInterface::create(const char *outputFilename) {
  KASSERT(!g_instance);
  g_instance = new ResourceManager(outputFilename);
  return true;
}

bool ResourceInterface::close() {
  delete exch_null(g_instance);
  return true;
}

void ResourceInterface::add_file_watch(const char *filename, void *token, const cbFileChanged &cb, 
                                       bool initial_callback, bool *initial_result, int timeout) {
  if (initial_callback) {
    bool res = cb(filename, token);
    if (initial_result)
      *initial_result = res;
  }
}

