#pragma once

// The resource manager is a file system abstraction, allowing
// easy use of dropbox etc while developing, and a zipfile or something
// for the final release

#if WITH_UNPACKED_RESOUCES
#include "resource_manager.hpp"
#define RESOURCE_MANAGER ResourceManager::instance()
#else
#include "packed_resource_manager.hpp"
#define RESOURCE_MANAGER PackedResourceManager::instance()
#endif

