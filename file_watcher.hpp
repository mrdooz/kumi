#pragma once

#include "utils.hpp"

using std::string;
using std::map;
using std::vector;
using std::deque;
using std::tuple;


enum FileEvent {
	kFileEventUnknown,
	kFileEventCreate,
	kFileEventModify,
	kFileEventDelete,
	kFileEventRename,
};

class FileWatcher {
public:

	typedef FastDelegate3<FileEvent /*event*/, const char * /*old_name*/, const char * /*new_name*/> FileChanged;

	static FileWatcher &instance();
	void add_file_watch(const char *filename, const FileChanged &fn);
	void remove_watch(const FileChanged &fn);
	void close();
private:
	FileWatcher();
	static FileWatcher *_instance;

	bool lazy_create_worker_thread();

	HANDLE _thread;
};

#define FILE_WATCHER FileWatcher::instance()
