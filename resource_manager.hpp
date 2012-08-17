#pragma once

#include "resource_interface.hpp"
#include "file_watcher.hpp"

using std::vector;
using std::string;

class ResourceManager : public ResourceInterface {
public:
  ResourceManager(const char *outputFilename);
  ~ResourceManager();

  bool file_exists(const char *filename);
  __time64_t mdate(const char *filename);
  virtual bool load_file(const char *filename, std::vector<char> *buf);
  virtual bool load_partial(const char *filename, size_t ofs, size_t len, std::vector<char> *buf);
  virtual bool load_inplace(const char *filename, size_t ofs, size_t len, void *buf);

  virtual bool supports_file_watch() const;
  virtual void add_file_watch(const char *filename, void *token, const cbFileChanged &cb, bool initial_callback, bool *initial_result, int timeout) override;
  virtual void remove_file_watch(const cbFileChanged &cb);

  void copy_on_load(bool enable, const char *dest);
  void add_path(const std::string &path);

  std::string resolve_filename(const char *filename, bool fullPath);

private:
  void file_changed(int timeout, void *token, FileWatcher::FileEvent, const std::string &old_name, const std::string &new_name);
  void deferred_file_changed(void *token, FileWatcher::FileEvent, const std::string &old_name, const std::string &new_name);

  static ResourceManager *_instance;
  bool _copy_on_load;
  std::string _copy_dest;
  std::vector<std::string> _paths;
  std::map<std::string, std::string> _resolved_paths;

  std::map<std::string, vector<std::pair<cbFileChanged, void*>>> _watched_files;

  std::map<std::string, int> _file_change_ref_count;

  std::string _outputFilename;
  std::set<std::string> _readFiles;
};

