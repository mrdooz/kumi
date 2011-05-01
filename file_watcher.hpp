#pragma once

enum FileEvent {
	kFileEventUnknown,
	kFileEventCreate,
	kFileEventModify,
	kFileEventDelete,
	kFileEventRename,
};

typedef FastDelegate3<FileEvent /*event*/, const char * /*old_name*/, const char * /*new_name*/> CbFileChanged;

class FileWatcher {
public:
	static FileWatcher &instance();
	void add_file_watch(const char *filename, const CbFileChanged &fn);
	void remove_watch(const CbFileChanged &fn);
	void close();
private:
	FileWatcher();
	bool lazy_create_worker_thread();

	static FileWatcher *_instance;
	HANDLE _thread;
};

#define FILE_WATCHER FileWatcher::instance()
