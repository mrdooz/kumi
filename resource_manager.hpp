#pragma once

// The resource manager is a file system abstraction, allowing
// easy use of dropbox etc while developing, and a zipfile or something
// for the final release

#include "resource_interface.hpp"

using std::vector;
using std::string;

class ResourceManager : public ResourceInterface {
public:
  ResourceManager();

  static bool create();
  static ResourceManager &instance();

  bool file_exists(const char *filename);
  __time64_t mdate(const char *filename);
  virtual bool load_partial(const char *filename, size_t ofs, size_t len, void **buf);
  virtual bool load_file(const char *filename, void **buf, size_t *len);

  virtual bool supports_file_watch() const;
  virtual void watch_file(const char *filename, bool initial_callback, const cbFileChanged &cb);

  void copy_on_load(bool enable, const char *dest);
  void add_path(const char *path);

  string resolve_filename(const char *filename);

private:
  static ResourceManager *_instance;
  bool _copy_on_load;
  string _copy_dest;
  vector<string> _paths;
  map<string, string> _resolved_paths;
};

#define RESOURCE_MANAGER ResourceManager::instance()
