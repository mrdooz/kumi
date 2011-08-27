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

typedef std::function<void (void * /*token*/, FileEvent /*event*/, const string & /*old_name*/, const string & /*new_name*/)> CbFileChanged;
//typedef FastDelegate4<void * /*token*/, FileEvent /*event*/, const char * /*old_name*/, const char * /*new_name*/> CbFileChanged;

struct WatcherThread;

class FileWatcher {
public:
	static FileWatcher &instance();
	void add_file_watch(const char *filename, void *token, bool initial_load, threading::ThreadId thread_id, const CbFileChanged &fn);
	void remove_watch(const CbFileChanged &fn);
	void close();
private:
	FileWatcher();
	bool lazy_create_worker_thread();

	static FileWatcher *_instance;
	WatcherThread *_thread;
};

#define FILE_WATCHER FileWatcher::instance()
