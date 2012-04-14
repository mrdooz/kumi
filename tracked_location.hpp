#pragma once

struct TrackedLocation {
  TrackedLocation() {}
  TrackedLocation(const char *function, const char *file, int line) : function(function), file(file), line(line) {}
  std::string function;
  std::string file;
  int line;
};

#define FROM_HERE TrackedLocation(__FUNCTION__, __FILE__, __LINE__)