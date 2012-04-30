#include "stdafx.h"
#include "utils.hpp"
#include "file_utils.hpp"
#include "resource_manager.hpp"
#include "file_watcher.hpp"
#include "threading.hpp"
#include "logger.hpp"

using namespace std::tr1::placeholders;
using namespace std;

ResourceManager *ResourceManager::_instance = nullptr;

static bool file_exists(const char *filename)
{
  if (_access(filename, 0) != 0)
    return false;

  struct _stat status;
  if (_stat(filename, &status) != 0)
    return false;

  return !!(status.st_mode & _S_IFREG);
}

static string normalize_path(const char *path, bool add_trailing_slash) {
  string res(path);
  for (size_t i = 0; i < res.size(); ++i) {
    if (res[i] == '\\')
      res[i] = '/';
  }

  if (add_trailing_slash) {
    if (!res.empty() && res.back() != '/')
      res.push_back('/');
  }

  return res;
}


ResourceManager::ResourceManager() 
  : _copy_on_load(false)
{
  _paths.push_back("./");
}

ResourceManager &ResourceManager::instance() {
  assert(_instance);
  return *_instance;
}

bool ResourceManager::create() {
  assert(!_instance);
  _instance = new ResourceManager();
  return true;
}

void ResourceManager::add_path(const char *path) {
  _paths.push_back(normalize_path(path, true));
}

bool ResourceManager::load_file(const char *filename, std::vector<uint8> *buf) {
  const string &full_path = resolve_filename(filename);
  if (full_path.empty()) return false;

  return ::load_file(full_path.c_str(), buf);
}
/*
bool ResourceManager::load_file(const char *filename, void **buf, size_t *len) {
  const string &full_path = resolve_filename(filename);
  if (full_path.empty()) return false;

  return ::load_file(full_path.c_str(), buf, len);
}
*/
bool ResourceManager::load_partial(const char *filename, size_t ofs, size_t len, std::vector<uint8> *buf) {
  buf->resize(len);
  return load_inplace(filename, ofs, len, buf->data());
}

bool ResourceManager::load_inplace(const char *filename, size_t ofs, size_t len, void *buf) {
  const string &full_path = resolve_filename(filename);

  ScopedHandle h(CreateFileA(full_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 
    FILE_ATTRIBUTE_NORMAL, NULL));
  if (h.handle() == INVALID_HANDLE_VALUE)
    return false;

  DWORD size = GetFileSize(h, NULL);
  if (ofs + len > size)
    return false;

  DWORD res;
  if (SetFilePointer(h, ofs, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    return false;
  }

  if (!ReadFile(h, buf, len, &res, NULL)) {
    DWORD err = GetLastError();
    return false;
  }
  return true;
}

bool ResourceManager::file_exists(const char *filename) {
  return !resolve_filename(filename).empty();
}

__time64_t ResourceManager::mdate(const char *filename) {
  struct _stat s;
  _stat(filename, &s);
  return s.st_mtime;

}

bool ResourceManager::supports_file_watch() const {
#ifdef _DISTRIBUTION_
  return false;
#else
  return true;
#endif
}

void ResourceManager::copy_on_load(bool enable, const char *dest) {
  _copy_on_load = enable;
  if (enable)
    _copy_dest;
}

string ResourceManager::resolve_filename(const char *filename) {

  if (::file_exists(filename)) {
    return normalize_path(filename, false);
  }

  auto it = _resolved_paths.find(filename);
  if (it != _resolved_paths.end())
    return it->second;
  string res;
#if _DEBUG
  // warn about duplicates
  int count = 0;
  for (size_t i = 0; i < _paths.size(); ++i) {
    string cand(_paths[i] + filename);
    if (::file_exists(cand.c_str())) {
      count++;
      if (res.empty())
        res = cand.c_str();
    }
  }
  if (count > 1)
    LOG_WARNING_LN("Multiple paths resolved for file: %s", filename);
#else
  for (size_t i = 0; i < _paths.size(); ++i) {
    string cand(_paths[i] + filename);
    if (::file_exists(cand.c_str())) {
      res = cand.c_str();
      break;
    }
  }
#endif
  if (!res.empty()) {
    res = normalize_path(res.c_str(), false);
    _resolved_paths[filename] = res;
  }
  return res;
}

void ResourceManager::add_file_watch(const char *filename, bool initial_callback, void *token, const cbFileChanged &cb) {
  if (initial_callback)
    cb(filename, token);

  FILE_WATCHER.add_file_watch(filename, token, threading::kMainThread, 
    bind(&ResourceManager::file_changed, this, _1, _2, _3, _4));

  _watched_files[filename].push_back(make_pair(cb, token));
}

void ResourceManager::remove_file_watch(const cbFileChanged &cb) {
  for (auto i = begin(_watched_files); i != end(_watched_files); ++i) {
    auto &v = i->second;
    v.erase(remove_if(begin(v), end(v), [&](const pair<cbFileChanged, void*> &x) { return cb == x.first; }), end(v));
  }
}

void ResourceManager::deferred_file_changed(void *token, FileWatcher::FileEvent event, const string &old_name, const string &new_name) {
  if (0 == --_file_change_ref_count[old_name]) {
    if ((uint32)event & (FileWatcher::kFileEventCreate | FileWatcher::kFileEventModify)) {
      auto i = _watched_files.find(old_name);
      if (i == end(_watched_files))
        return;
      // call all the callbacks registered for the changed file
      auto &v = i->second;
      for (auto j = begin(v); j != end(v); ++j) {
        const pair<cbFileChanged, void*> &x = *j;
        x.first(old_name.c_str(), x.second);
      }
    }
  }
}

void ResourceManager::file_changed(void *token, FileWatcher::FileEvent event, const string &old_name, const string &new_name) {
  _file_change_ref_count[old_name]++;
  DISPATCHER.invoke_in(FROM_HERE, threading::kMainThread, 2500, 
    bind(&ResourceManager::deferred_file_changed, this, token, event, old_name, new_name));
}
