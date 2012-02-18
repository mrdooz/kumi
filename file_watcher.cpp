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
  DeferredFileEvent(DirWatch *watch, FileEvent event, uint32 delta, const string &filename, const string &new_name = string()) 
    : watch(watch), event(event), delta(delta), filename(filename), new_name(new_name) {}
  DirWatch *watch;
  FileEvent event;
  uint32 delta;
  string filename;
  string new_name;
};

struct DirWatch {
  DirWatch(const string &path, HANDLE handle) 
    : _path(path)
    , _handle(handle) { 
    ZeroMemory(&_overlapped, sizeof(_overlapped)); 
    _overlapped.hEvent = this;
  }
  void on_completion();
  bool start_watch();
  string _path;
  HANDLE _handle;
  uint8_t _buf[4096];
  OVERLAPPED _overlapped;
  unordered_map<string, DWORD> _last_event;
  struct Callback {
    Callback(ThreadId id, const CbFileChanged &cb, void *token) : thread(id), cb(cb), token(token) {}
    ThreadId thread;
    CbFileChanged cb;
    void *token;
  };
  typedef map<string, vector<Callback> > FileWatches;
  FileWatches _file_watches;
};

typedef tuple<CbFileChanged, HANDLE /*event*/> DeferredRemove;
typedef tuple<string /*filename*/, CbFileChanged, void * /*token*/> DeferredAdd;

class WatcherThread : public SleepyThread {
public:
  WatcherThread();
  ~WatcherThread();
  void add_watch(const string &fullname, ThreadId thread_id, const CbFileChanged &cb, void *token);
  void remove_watch(const CbFileChanged &cb);
  void terminate();
private:
  friend struct DirWatch;
  virtual UINT run(void *userdata);
  static bool is_watching_file(const char *filename);


  static void CALLBACK on_completion(DWORD error_code, DWORD bytes_transfered, OVERLAPPED *overlapped);

  static unordered_map<string, DeferredFileEvent> _events;
  static map<string, int> _watched_files;

  typedef map<string, DirWatch *> DirWatches;
  static DirWatches _dir_watches;

  static bool _terminating;
};

map<string, int> WatcherThread::_watched_files;
WatcherThread::DirWatches WatcherThread::_dir_watches;
unordered_map<string, DeferredFileEvent> WatcherThread::_events;
bool WatcherThread::_terminating = false;

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

      if (WatcherThread::is_watching_file(fullname.c_str()) || info->Action == FILE_ACTION_RENAMED_NEW_NAME) {

        switch (info->Action) {

        case FILE_ACTION_ADDED:
          WatcherThread::_events[fullname] = DeferredFileEvent(this, kFileEventCreate, delta, fullname);
          break;

        case FILE_ACTION_MODIFIED:
          WatcherThread::_events[fullname] = DeferredFileEvent(this, kFileEventModify, delta, fullname);
          break;

        case FILE_ACTION_REMOVED:
          WatcherThread::_events[fullname] = DeferredFileEvent(this, kFileEventDelete, delta, fullname);
          break;

        case FILE_ACTION_RENAMED_OLD_NAME:
          old_name = filename;
          break;

        case FILE_ACTION_RENAMED_NEW_NAME:
          if (!old_name.empty()) {
            WatcherThread::_events[fullname] = DeferredFileEvent(this, kFileEventRename, delta, old_name, filename);
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
  return !!ReadDirectoryChangesW(_handle, _buf, sizeof(_buf), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, 
    NULL, &_overlapped, WatcherThread::on_completion);
}

WatcherThread::WatcherThread() {
  DISPATCHER.set_thread(kFileMonitorThread, this);
}

WatcherThread::~WatcherThread() {
}

bool WatcherThread::is_watching_file(const char *filename)
{
  auto it = _watched_files.find(filename);
  return it != _watched_files.end() && it->second > 0;
}

UINT WatcherThread::run(void *userdata)
{
  const DWORD cFileInactivity = 500;

  DWORD sleep_interval = INFINITE;
  while (true) {
    DWORD res = WaitForSingleObjectEx(_deferred_event, sleep_interval, TRUE);
    static DWORD last_tick = timeGetTime();
    process_deferred();
    if (_terminating)
      break;

    const DWORD now = timeGetTime();
    DWORD delta = now - last_tick;
    last_tick = now;

    sleep_interval = INFINITE;

    for (auto i = _events.begin(); i != _events.end(); ) {
      const DeferredFileEvent &cur = i->second;
      DirWatch *watch = cur.watch;
      if (i->second.delta > cFileInactivity) {
        auto it_file = watch->_file_watches.find(cur.filename);
        if (it_file != watch->_file_watches.end()) {
          // invoke all the callbacks on the current file
          for (auto it_callback = it_file->second.begin(); it_callback != it_file->second.end(); ++it_callback) {
            const DirWatch::Callback &cb = *it_callback;
            DISPATCHER.invoke(FROM_HERE, cb.thread, NAMED_CALLBACK(boost::bind(cb.cb, cb.token, cur.event, cur.filename, cur.new_name)));
          }
        }
        i = _events.erase(i);

      } else {
        sleep_interval = min(sleep_interval, i->second.delta);
        ++i;
      }
    }
  }
  return 0;
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

void WatcherThread::terminate()
{
  _terminating = true;
  for (auto i = _dir_watches.begin(); i != _dir_watches.end(); ++i) {
    DirWatch *watch = i->second;
    CancelIo(watch->_handle);
  }
  _dir_watches.clear();
}

void WatcherThread::add_watch(const string &fullname, ThreadId thread_id, const CbFileChanged &cb, void *token) {

  _watched_files[fullname]++;

  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath(fullname.c_str(), drive, dir, fname, ext);
  string path(string(drive) + dir);
  string filename(string(fname) + ext);

  // Check if we need to create a new watch for the directory
  DirWatches &dir_watches = _dir_watches;
  if (dir_watches.find(path) == dir_watches.end()) {

    HANDLE h = CreateFileA(
      path.c_str(),
      FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
      NULL);

    if (h == INVALID_HANDLE_VALUE)
      return;

    DirWatch *w = new DirWatch(path, h);

    // add a new file watch
    if (!w->start_watch()) {
      delete w;
      return;
    }

    dir_watches[path] = w;
  }

  dir_watches[path]->_file_watches[fullname].push_back(DirWatch::Callback(thread_id, cb, token));
}

void WatcherThread::remove_watch(const CbFileChanged &cb)
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
    if (!_thread->start(this))
      return;
  }

  DISPATCHER.invoke_and_wait(FROM_HERE, kFileMonitorThread, 
                             NAMED_CALLBACK(bind(&WatcherThread::add_watch, _thread, filename, thread_id, fn, token)));
}

void FileWatcher::remove_watch(const CbFileChanged &fn) {
  assert(_thread);
  if (!_thread)
    return;

  DISPATCHER.invoke_and_wait(FROM_HERE, kFileMonitorThread, 
                             NAMED_CALLBACK(bind(&WatcherThread::remove_watch, _thread, fn)));
}

void FileWatcher::close() {
  if (!_thread)
    return;

  DISPATCHER.invoke_and_wait(FROM_HERE, kFileMonitorThread, 
                             NAMED_CALLBACK(boost::bind(&WatcherThread::terminate, _thread)));
  delete exch_null(_thread);
}
