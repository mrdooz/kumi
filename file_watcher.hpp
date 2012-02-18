#pragma once

namespace threading {
  class Thread;
  enum ThreadId;
}

enum FileEvent {
  kFileEventUnknown,
  kFileEventCreate,
  kFileEventModify,
  kFileEventDelete,
  kFileEventRename,
};

// token, event, old_name, new_name
typedef std::function<void (void *, FileEvent, const string &, const string & )> CbFileChanged;

class WatcherThread;

class FileWatcher {
public:
  static FileWatcher &instance();
  void add_file_watch(const char *filename, void *token, threading::ThreadId thread_id, const CbFileChanged &fn);
  void remove_watch(const CbFileChanged &fn);
  void close();
private:
  FileWatcher();

  static FileWatcher *_instance;
  WatcherThread *_thread;
};

#define FILE_WATCHER FileWatcher::instance()
