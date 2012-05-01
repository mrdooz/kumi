#include "stdafx.h"
#include "file_watcher.hpp"
#include "utils.hpp"
#include "string_utils.hpp"
#include "threading.hpp"
#include <boost/bind.hpp>

#pragma comment(lib, "winmm.lib")

using std::pair;
using std::make_pair;
using std::string;
using std::map;
using std::vector;
using std::deque;
using std::tuple;
using std::make_tuple;
using std::get;
using boost::scoped_ptr;

using namespace threading;
using namespace std;

struct DirWatch;

struct DeferredFileEvent {
  DeferredFileEvent() {}
  DeferredFileEvent(DirWatch *watch, FileWatcher::FileEvent event, uint32 delta, const string &filename, 
                    const string &new_name = string()) 
    : watch(watch), event(event), delta(delta), filename(filename), new_name(new_name) {}
  DirWatch *watch;
  FileWatcher::FileEvent event;
  uint32 delta;
  string filename;
  string new_name;
};

struct DirWatch {
  DirWatch(const string &path, HANDLE handle, WatcherThread *watcher)
    : _path(path)
    , _handle(handle)
    , _watcher(watcher) { 
    ZeroMemory(&_overlapped, sizeof(_overlapped)); 
    _overlapped.hEvent = this;
  }

  ~DirWatch() {
    CloseHandle(_handle);
  }

  void on_completion();
  bool start_watch();

  string _path;
  HANDLE _handle;
  uint8_t _buf[4096];
  OVERLAPPED _overlapped;
  unordered_map<string, DWORD> _last_event;

  struct Callback {
    Callback(ThreadId id, const FileWatcher::CbFileChanged &cb, void *token) : thread(id), cb(cb), token(token) {}
    ThreadId thread;
    FileWatcher::CbFileChanged cb;
    void *token;
  };

  typedef map<string, vector<Callback> > FileWatches;
  FileWatches _file_watches;
  WatcherThread *_watcher;
};

class WatcherThread : public SleepyThread {
public:
  WatcherThread();
  ~WatcherThread();
  void add_watch(const string &fullname, ThreadId thread_id, const FileWatcher::CbFileChanged &cb, void *token);
  void remove_watch(const FileWatcher::CbFileChanged &cb);
  virtual void on_idle();
  virtual void has_joined();
private:
  friend struct DirWatch;
  bool is_watching_file(const char *filename);

  static void CALLBACK on_completion(DWORD error_code, DWORD bytes_transfered, OVERLAPPED *overlapped);

  unordered_map<string, DeferredFileEvent> _events;

  typedef string Filename;
  typedef int RefCount;
  map<Filename, RefCount> _watched_files;

  map<string, unique_ptr<DirWatch> > _dir_watches;
  DWORD _last_tick;
};

FileWatcher *FileWatcher::_instance = nullptr;

static bool is_direct_action(DWORD action) {
  return action == FILE_ACTION_ADDED ||
    action == FILE_ACTION_MODIFIED ||
    action == FILE_ACTION_REMOVED;
}

void DirWatch::on_completion()
{
  byte *base = _buf;

  const DWORD now = timeGetTime();

  string filename, old_name;
  while (true) {
    FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)base;
    if (wide_char_to_utf8(info->FileName, info->FileNameLength / 2, &filename)) {
      const string &fullname = _path + filename;

      DWORD delta = now - _last_event[fullname];
      _last_event[fullname] = now;

      if (_watcher->is_watching_file(fullname.c_str()) || info->Action == FILE_ACTION_RENAMED_NEW_NAME) {

        switch (info->Action) {

          case FILE_ACTION_ADDED:
            _watcher->_events[fullname] = DeferredFileEvent(this, FileWatcher::kFileEventCreate, delta, fullname);
            break;

          case FILE_ACTION_MODIFIED:
            _watcher->_events[fullname] = DeferredFileEvent(this, FileWatcher::kFileEventModify, delta, fullname);
            break;

          case FILE_ACTION_REMOVED:
            _watcher->_events[fullname] = DeferredFileEvent(this, FileWatcher::kFileEventDelete, delta, fullname);
            break;

          case FILE_ACTION_RENAMED_OLD_NAME:
            old_name = filename;
            break;

          case FILE_ACTION_RENAMED_NEW_NAME:
            if (!old_name.empty()) {
              _watcher->_events[fullname] = DeferredFileEvent(this, FileWatcher::kFileEventRename, delta, old_name, 
                                                              filename);
              old_name.clear();
            }
            break;
        }

      }
    }

    if (info->NextEntryOffset == 0)
      break;
    base += info->NextEntryOffset;
  }
}

bool DirWatch::start_watch()
{
  return !!ReadDirectoryChangesW(_handle, _buf, sizeof(_buf), FALSE, 
                                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, 
                                 NULL, &_overlapped, WatcherThread::on_completion);
}

WatcherThread::WatcherThread() 
  : SleepyThread(kFileMonitorThread)
  , _last_tick(-1) 
{
}

WatcherThread::~WatcherThread() {
}

bool WatcherThread::is_watching_file(const char *filename)
{
  auto it = _watched_files.find(filename);
  return it != _watched_files.end() && it->second > 0;
}

void WatcherThread::on_idle() {

  const int cFileInactivity = 500;

  const DWORD now = timeGetTime();
  if (_last_tick == -1)
    _last_tick = now;
  DWORD delta = now - _last_tick;
  _last_tick = now;

  for (auto i = begin(_events); i != end(_events); ) {
    const DeferredFileEvent &cur = i->second;
    DirWatch *watch = cur.watch;
    if (i->second.delta > cFileInactivity) {
      auto it_file = watch->_file_watches.find(cur.filename);
      if (it_file == watch->_file_watches.end())
        break;
      // invoke all the callbacks on the current file
      for (auto it_callback = begin(it_file->second); it_callback != end(it_file->second); ++it_callback) {
        const DirWatch::Callback &cb = *it_callback;
        DISPATCHER.invoke(FROM_HERE, cb.thread, bind(cb.cb, cb.token, cur.event, cur.filename, cur.new_name));
      }
      i = _events.erase(i);

    } else {
      _sleep_interval = std::min<DWORD>(_sleep_interval, i->second.delta);
      ++i;
    }
  }
}

void WatcherThread::on_completion(DWORD error_code, DWORD bytes_transfered, OVERLAPPED *overlapped)
{
  DirWatch *d = (DirWatch *)(overlapped->hEvent);
  if (error_code == ERROR_OPERATION_ABORTED) {
    delete d;
    return;
  }

  if (error_code != ERROR_SUCCESS || bytes_transfered == 0)
    return;

  d->on_completion();
  d->start_watch();
}

void WatcherThread::has_joined() {
  for (auto i = begin(_dir_watches); i != end(_dir_watches); ++i) {
    auto &watch = i->second;
    CancelIo(watch->_handle);
  }
  _dir_watches.clear();
}

void WatcherThread::add_watch(const string &fullname, ThreadId thread_id, const FileWatcher::CbFileChanged &cb, 
                              void *token) {
  _watched_files[fullname]++;

  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath(fullname.c_str(), drive, dir, fname, ext);
  string path(string(drive) + dir);
  string filename(string(fname) + ext);

  // Check if we need to create a new watch for the directory
  auto &dir_watches = _dir_watches;
  if (dir_watches.find(path) == dir_watches.end()) {

    HANDLE h = CreateFileA(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (h == INVALID_HANDLE_VALUE)
      return;

    DirWatch *w = new DirWatch(path, h, this);

    // add a new file watch
    if (!w->start_watch()) {
      delete w;
      return;
    }

    dir_watches[path].reset(w);
  }

  dir_watches[path]->_file_watches[fullname].push_back(DirWatch::Callback(thread_id, cb, token));
}

void WatcherThread::remove_watch(const FileWatcher::CbFileChanged &cb)
{
  /*
  // dec the ref count on all the files watches by this callback
  auto it = _files_by_callback.find(fn);
  if (it != _files_by_callback.end()) {
  for (auto j = it->second.begin(); j != it->second.end(); ++j) {
  auto tmp = _watched_files.find(*j);
  if (--tmp->second == 0)
  _watched_files.erase(tmp);
  }
  _files_by_callback.erase(it);
  } else {
  SetEvent(event);
  return;
  }

  for (auto it_dir = _dir_watches.begin(); it_dir != _dir_watches.end(); ++it_dir) {
  DirWatch *d = it_dir->second;
  for (auto it_file = d->_file_watches.begin(); it_file != d->_file_watches.end(); ++it_file) {
  for (auto it_callback = it_file->second.begin(); it_callback != it_file->second.end(); ) {

  if (it_callback->first == fn) {
  it_callback = it_file->second.erase(it_callback);
  } else {
  ++it_callback;
  }
  }
  }
  }
  SetEvent(event);
  */
}

FileWatcher::FileWatcher() 
  : _thread(nullptr) 
{
}

FileWatcher &FileWatcher::instance() {
  if (!_instance)
    _instance = new FileWatcher;
  return *_instance;
}

void FileWatcher::add_file_watch(const char *filename, void *token, ThreadId thread_id, const CbFileChanged &fn) {
  if (!_thread) {
    _thread = new WatcherThread;
    if (!_thread->start())
      return;
  }

  DISPATCHER.invoke_and_wait(FROM_HERE, kFileMonitorThread, 
                             bind(&WatcherThread::add_watch, _thread, filename, thread_id, fn, token));
}

void FileWatcher::remove_watch(const CbFileChanged &fn) {
  assert(_thread);
  if (!_thread)
    return;

  DISPATCHER.invoke_and_wait(FROM_HERE, kFileMonitorThread, bind(&WatcherThread::remove_watch, _thread, fn));
}

void FileWatcher::close() {
  if (!_instance)
    return;

  if (_instance->_thread) {
    _instance->_thread->join();
    delete exch_null(_instance->_thread);
  }

  delete exch_null(_instance);
}
