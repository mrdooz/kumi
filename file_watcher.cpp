#include "stdafx.h"
#include "file_watcher.hpp"

#pragma comment(lib, "winmm.lib")

struct DirWatch;

struct DeferredFileEvent {
	DeferredFileEvent(DirWatch *watch, FileEvent event, uint32_t time, const string &filename, const string &new_name = "") : watch(watch), event(event), time(time), filename(filename), new_name(new_name) {}
	DirWatch *watch;
	FileEvent event;
	uint32_t time;
	string filename;
	string new_name;
};

struct DirWatch {
	DirWatch(const string &path, HANDLE handle) : _path(path), _handle(handle) { ZeroMemory(&_overlapped, sizeof(_overlapped)); _overlapped.hEvent = this;}
	void on_completion();
	bool start_watch();
	string _path;
	HANDLE _handle;
	uint8_t _buf[4096];
	OVERLAPPED _overlapped;
	typedef map<string, vector<FileWatcher::FileChanged> > FileWatches;
	FileWatches _file_watches;
};


using std::pair;
using boost::scoped_ptr;

FileWatcher *FileWatcher::_instance = nullptr;

FileWatcher &FileWatcher::instance()
{
	if (!_instance)
		_instance = new FileWatcher;
	return *_instance;
}

struct WatcherThread {

	static UINT CALLBACK thread_proc(void *userdata);
	static bool is_watching_file(const char *filename);

	static void CALLBACK add_watch_apc(ULONG_PTR data);
	static void CALLBACK remove_watch_apc(ULONG_PTR data);
	static void CALLBACK terminate_apc(ULONG_PTR data);
	static void CALLBACK on_completion(DWORD error_Code, DWORD bytes_transfered, OVERLAPPED *overlapped);

	static map<string, int> _deferred_file_events_ref_count;

	typedef std::pair<FileWatcher::FileChanged, HANDLE /*event*/> DeferredRemove;
	typedef std::pair<string /*filename*/, FileWatcher::FileChanged> DeferredAdd;

	static map<string, int> _watched_files;
	static map<FileWatcher::FileChanged, vector<string> > _files_by_callback;

	typedef map<string, DirWatch *> DirWatches;
	static DirWatches _dir_watches;

	static deque<DeferredFileEvent> _events;

	static bool _terminating;
};

map<string, int> WatcherThread::_deferred_file_events_ref_count;
map<string, int> WatcherThread::_watched_files;
map<FileWatcher::FileChanged, vector<string> > WatcherThread::_files_by_callback;
WatcherThread::DirWatches WatcherThread::_dir_watches;
deque<DeferredFileEvent> WatcherThread::_events;
bool WatcherThread::_terminating = false;

bool WatcherThread::is_watching_file(const char *filename)
{
	auto it = _watched_files.find(filename);
	return it != _watched_files.end() && it->second > 0;
}

void DirWatch::on_completion()
{
	byte *base = _buf;

	// to avoid multiple callbacks when doing operations that generate multiple file events (like
	// copying large files), we ref count the file events
	const DWORD now = timeGetTime();

	string filename, old_name;
	while (true) {
		FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)base;
		if (wide_char_to_utf8(info->FileName, info->FileNameLength / 2, &filename)) {
			const string fullname = _path + filename;
			switch (info->Action) {
			case FILE_ACTION_ADDED:
				{
					if (WatcherThread::is_watching_file(fullname.c_str())) {
						WatcherThread::_events.push_back(DeferredFileEvent(this, kFileEventCreate, now, filename));
						WatcherThread::_deferred_file_events_ref_count[filename]++;
					}
				}
				break;
			case FILE_ACTION_MODIFIED:
				{
					if (WatcherThread::is_watching_file(fullname.c_str())) {
						WatcherThread::_events.push_back(DeferredFileEvent(this, kFileEventModify, now, filename));
						WatcherThread::_deferred_file_events_ref_count[filename]++;
					}
				}
				break;
			case FILE_ACTION_REMOVED:
				{
					if (WatcherThread::is_watching_file(fullname.c_str())) {
						WatcherThread::_events.push_back(DeferredFileEvent(this, kFileEventDelete, now, filename));
						WatcherThread::_deferred_file_events_ref_count[filename]++;
					}
				}
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
				{
					if (WatcherThread::is_watching_file(fullname.c_str())) {
						old_name = filename;
					}
				}
				break;
			case FILE_ACTION_RENAMED_NEW_NAME:
				if (!old_name.empty()) {
					WatcherThread::_events.push_back(DeferredFileEvent(this, kFileEventRename, now, old_name, filename));
					WatcherThread::_deferred_file_events_ref_count[old_name]++;
					old_name.clear();
				}
				break;
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

FileWatcher::FileWatcher() 
	: _thread(INVALID_HANDLE_VALUE) 
{
}


UINT WatcherThread::thread_proc(void *userdata)
{
	const DWORD cFileInactivity = 500;

	while (true) {
		DWORD res = SleepEx(250, TRUE);
		if (_terminating)
			break;

		const DWORD now = timeGetTime();

		// process any deferred file events
		while (!_events.empty()) {
			const DeferredFileEvent &cur = _events.front();

			if (now - cur.time < cFileInactivity) {
				break;
			}

			DirWatch *watch = cur.watch;
			if (--_deferred_file_events_ref_count[cur.filename] == 0) {
				auto it_file = watch->_file_watches.find(cur.filename);
				if (it_file != watch->_file_watches.end()) {
					// invoke all the callbacks on the current file
					for (auto it_callback = it_file->second.begin(); it_callback != it_file->second.end(); ++it_callback)
						(*it_callback)(cur.event, cur.filename.c_str(), cur.new_name.c_str());
				}
			}
			_events.pop_front();
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

	if (error_code != ERROR_SUCCESS || !bytes_transfered)
		return;

	d->on_completion();
	d->start_watch();
}

void WatcherThread::terminate_apc(ULONG_PTR data)
{
	_terminating = true;
	for (auto i = _dir_watches.begin(); i != _dir_watches.end(); ++i) {
		DirWatch *watch = i->second;
		CancelIo(watch->_handle);
	}
	_dir_watches.clear();
}

void WatcherThread::add_watch_apc(ULONG_PTR data)
{
	scoped_ptr<DeferredAdd> cur((DeferredAdd *)data);
	const string &fullname = std::get<0>(*cur);
	const FileWatcher::FileChanged &fn = std::get<1>(*cur);

	_watched_files[fullname]++;
	_files_by_callback[fn].push_back(fullname);

	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	_splitpath(fullname.c_str(), drive, dir, fname, ext);
	string path(string(drive) + dir);
	string filename(string(fname) + ext);

	// Check if we need to create a new watch
	DirWatches &dir_watches = _dir_watches;
	if (dir_watches.find(path) == dir_watches.end()) {

		HANDLE h = CreateFile(
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

	dir_watches[path]->_file_watches[filename].push_back(fn);
}

void WatcherThread::remove_watch_apc(ULONG_PTR data)
{
	scoped_ptr<DeferredRemove> p((DeferredRemove *)data);
	FileWatcher::FileChanged fn = p->first;
	HANDLE event = p->second;

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
				if (*it_callback == fn) {
					it_callback = it_file->second.erase(it_callback);
				} else {
					++it_callback;
				}
			}
		}
	}
	SetEvent(event);
}

bool FileWatcher::lazy_create_worker_thread()
{
	if (_thread != INVALID_HANDLE_VALUE)
		return true;
	_thread = (HANDLE)_beginthreadex(NULL, 0, &WatcherThread::thread_proc, this, 0, NULL);
	return _thread != INVALID_HANDLE_VALUE;
}



void FileWatcher::add_file_watch(const char *filename, const FileChanged &fn)
{
	if (!lazy_create_worker_thread())
		return;

	QueueUserAPC(&WatcherThread::add_watch_apc, _thread, (ULONG_PTR)new WatcherThread::DeferredAdd(std::make_pair(filename, fn)));
}

void FileWatcher::remove_watch(const FileChanged &fn)
{
	if (_thread == INVALID_HANDLE_VALUE)
		return;

	HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
	QueueUserAPC(&WatcherThread::remove_watch_apc, _thread, (ULONG_PTR)new WatcherThread::DeferredRemove(std::make_pair(fn, h)));
	WaitForSingleObject(h, INFINITE);
}

void FileWatcher::close()
{
	if (_thread == INVALID_HANDLE_VALUE)
		return;

	QueueUserAPC(&WatcherThread::terminate_apc, _thread, (ULONG_PTR)this);
	WaitForSingleObject(_thread, INFINITE);
	_thread = INVALID_HANDLE_VALUE;
}
