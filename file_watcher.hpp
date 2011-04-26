#pragma once

#include "utils.hpp"

using std::string;
using std::map;
using std::vector;
using std::deque;


class FileWatcher {
public:
	enum FileEvent {
		kFileEventUnknown,
		kFileEventCreate,
		kFileEventModify,
		kFileEventDelete,
		kFileEventRename,
	};

	typedef FastDelegate3<FileEvent /*event*/, const char * /*old_name*/, const char * /*new_name*/> FileChanged;

	static FileWatcher &instance();
	void add_file_watch(const char *filename, const FileChanged &fn);
	void remove_watch(const FileChanged &fn);
	bool is_watching_file(const char *filename) const;
	void close();
private:
	FileWatcher();
	static FileWatcher *_instance;

	static UINT __stdcall threadProc(void *userdata);
	bool lazy_create_worker_thread();

	static void CALLBACK add_watch_apc(ULONG_PTR data);
	static void CALLBACK terminate_apc(ULONG_PTR data);
	static void CALLBACK on_completion(DWORD error_Code, DWORD bytes_transfered, OVERLAPPED *overlapped);

	struct DirWatch {
		DirWatch(const string &path, HANDLE handle) : _path(path), _handle(handle) { ZeroMemory(&_overlapped, sizeof(_overlapped)); _overlapped.hEvent = this;}
		void on_completion();
		bool start_watch();
		string _path;
		HANDLE _handle;
		uint8_t _buf[4096];
		OVERLAPPED _overlapped;
		typedef map<string, vector<FileChanged> > FileWatches;
		FileWatches _file_watches;
	};

	struct DeferredFileEvent {
		DeferredFileEvent(FileEvent event, uint32_t time, const string &path, const string &filename) : event(event), time(time), path(path), filename(filename) {}
		DeferredFileEvent(FileEvent event, uint32_t time, const string &path, const string &filename, const string &new_name) : event(event), time(time), path(path), filename(filename), new_name(new_name) {}
		FileEvent event;
		uint32_t time;
		string path;
		string filename;
		string new_name;
	};


	typedef deque<DeferredFileEvent> DeferredFileEvents;
	DeferredFileEvents _deferred_file_events;
	map<string, int> _deferred_file_events_ref_count;

	typedef deque< std::pair<string, FileChanged> > DeferredAdds;
	DeferredAdds _deferred_adds;
	map<string, int> _watched_files;

	typedef map<string, DirWatch *> DirWatches;
	DirWatches _dir_watches;

	bool _terminating;
	HANDLE _thread;
};

#define FILE_WATCHER FileWatcher::instance()
