#pragma once

namespace threading {
  class Thread;
  enum ThreadId;
}

class WatcherThread;

class FileWatcher {
public:

  enum FileEvent {
    kFileEventCreate = 1 << 0,
    kFileEventModify = 1 << 1,
    kFileEventDelete = 1 << 2,
    kFileEventRename = 1 << 3,
  };

  // token, event, old_name, new_name
  typedef std::function<void (void *, FileEvent, const std::string &, const std::string & )> CbFileChanged;

  static FileWatcher &instance();
  // TODO: use a mask to specify what event's we're interested in
  void add_file_watch(const char *filename, void *token, threading::ThreadId thread_id, const CbFileChanged &fn);
  void remove_watch(const CbFileChanged &fn);
  static void close();
private:
  FileWatcher();

  static FileWatcher *_instance;
  WatcherThread *_thread;
};

#define FILE_WATCHER FileWatcher::instance()
