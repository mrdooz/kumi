#pragma once

#include "utils.hpp"

using std::string;
using std::map;
using std::vector;
using std::deque;
using std::tuple;


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
	void close();
private:
	FileWatcher();
	static FileWatcher *_instance;

	static UINT __stdcall threadProc(void *userdata);
	bool lazy_create_worker_thread();

	// runs on worker thread
	struct DirWatch;
	bool is_watching_file(const char *filename) const;
	static void add_event(DirWatch *watch, FileEvent event, uint32_t time, const string &filename, const string &new_name = "");
	static void CALLBACK add_watch_apc(ULONG_PTR data);
	static void CALLBACK remove_watch_apc(ULONG_PTR data);
	static void CALLBACK terminate_apc(ULONG_PTR data);
	static void CALLBACK on_completion(DWORD error_Code, DWORD bytes_transfered, OVERLAPPED *overlapped);
	//---

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
		map<string, int> _deferred_file_events_ref_count;
	};

	struct DeferredFileEvent {
		DeferredFileEvent(DirWatch *watch, FileEvent event, uint32_t time, const string &filename) : watch(watch), event(event), time(time), filename(filename) {}
		DeferredFileEvent(DirWatch *watch, FileEvent event, uint32_t time, const string &filename, const string &new_name) : watch(watch), event(event), time(time), filename(filename), new_name(new_name) {}
		SLIST_ENTRY entry;
		DirWatch *watch;
		FileEvent event;
		uint32_t time;
		string filename;
		string new_name;
	};

	SLIST_HEADER *_list_head;

	map<string, int> _deferred_file_events_ref_count;


	typedef std::pair<FileWatcher *, FileChanged> DeferredRemove;
	typedef std::tuple<FileWatcher *, string /*filename*/, FileChanged> DeferredAdd;
	map<string, int> _watched_files;

	map<FileChanged, vector<string> > _files_by_callback;

	typedef map<string, DirWatch *> DirWatches;
	DirWatches _dir_watches;

	bool _terminating;
	HANDLE _thread;
};

#define FILE_WATCHER FileWatcher::instance()
