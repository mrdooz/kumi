#pragma once

#if WITH_UNPACKED_RESOUCES

#include "file_watcher.hpp"
#include "graphics_object_handle.hpp"

typedef std::function<bool (const char *, void *)> cbFileChanged;

class ResourceManager {
public:

  ResourceManager(const char *outputFilename);
  ~ResourceManager();

  static ResourceManager &instance();
  static bool create(const char *outputFilename);
  static bool close();

  bool file_exists(const char *filename);
  __time64_t mdate(const char *filename);
  bool load_file(const char *filename, std::vector<char> *buf);
  bool load_partial(const char *filename, size_t ofs, size_t len, std::vector<char> *buf);
  bool load_inplace(const char *filename, size_t ofs, size_t len, void *buf);
  GraphicsObjectHandle load_texture(const char *filename, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info);

  void add_file_watch(const char *filename, void *token, const cbFileChanged &cb, bool initial_callback, bool *initial_result, int timeout);
  void remove_file_watch(const cbFileChanged &cb);
  void add_path(const std::string &path);

private:
  std::string resolve_filename(const char *filename, bool fullPath);
  void file_changed(int timeout, void *token, FileWatcher::FileEvent, const std::string &old_name, const std::string &new_name);
  void deferred_file_changed(void *token, FileWatcher::FileEvent, const std::string &old_name, const std::string &new_name);

  std::vector<std::string> _paths;
  std::map<std::string, std::string> _resolved_paths;

  std::map<std::string, std::vector<std::pair<cbFileChanged, void*>>> _watched_files;

  std::map<std::string, int> _file_change_ref_count;

  std::string _outputFilename;

  struct FileInfo {
    FileInfo(const std::string &orgName, const std::string &resolvedName) : orgName(orgName), resolvedName(resolvedName) {}
    bool operator<(const FileInfo &rhs) const {
      if (orgName == rhs.orgName) {
        if (resolvedName == rhs.resolvedName) {
          return false;
        }
        return resolvedName < rhs.resolvedName;
      }
      return orgName < rhs.orgName;
    }
    std::string orgName;
    std::string resolvedName;
  };


  std::set<FileInfo> _readFiles;
};

#endif