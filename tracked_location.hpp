#pragma once

#if WITH_TRACKED_LOCATION
struct TrackedLocation {
  TrackedLocation() {}
  TrackedLocation(const char *function, const char *file, int line) : function(function), file(file), line(line) {}
  const char *function; // note, these are just const char*s, as they should come from __FUNCTION__ and __FILE__
  const char *file;
  int line;
};

#define FROM_HERE TrackedLocation(__FUNCTION__, __FILE__, __LINE__)
#else
struct TrackedLocation{};
#define FROM_HERE TrackedLocation()
#endif
