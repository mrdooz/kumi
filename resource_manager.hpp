#pragma once

// The resource manager is a file system abstraction, allowing
// easy use of dropbox etc while developing, and a zipfile or something
// for the final release

#include "resource_interface.hpp"
#include "file_watcher.hpp"

using std::vector;
using std::string;

class ResourceManager : public ResourceInterface {
public:
  ResourceManager();

  static bool create();
  static ResourceManager &instance();

  bool file_exists(const char *filename);
  __time64_t mdate(const char *filename);
  virtual bool load_file(const char *filename, std::vector<uint8> *buf);
  virtual bool load_partial(const char *filename, size_t ofs, size_t len, std::vector<uint8> *buf);
  virtual bool load_inplace(const char *filename, size_t ofs, size_t len, void *buf);

  virtual bool supports_file_watch() const;
  virtual void add_file_watch(const char *filename, bool initial_callback, void *token, const cbFileChanged &cb);
  virtual void remove_file_watch(const cbFileChanged &cb);

  void copy_on_load(bool enable, const char *dest);
  void add_path(const char *path);

  std::string resolve_filename(const char *filename);

private:
  void file_changed(void *token, FileWatcher::FileEvent, const std::string &old_name, const std::string &new_name);
  void deferred_file_changed(void *token, FileWatcher::FileEvent, const std::string &old_name, const std::string &new_name);

  static ResourceManager *_instance;
  bool _copy_on_load;
  std::string _copy_dest;
  std::vector<std::string> _paths;
  std::map<std::string, std::string> _resolved_paths;

  std::map<std::string, vector<std::pair<cbFileChanged, void*>>> _watched_files;

  std::map<std::string, int> _file_change_ref_count;
};

#define RESOURCE_MANAGER ResourceManager::instance()
