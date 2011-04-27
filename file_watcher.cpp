#include "stdafx.h"
#include "file_watcher.hpp"

#pragma comment(lib, "winmm.lib")

#define ALIGNED_MALLOC(type) (type *)_aligned_malloc(sizeof(type), MEMORY_ALLOCATION_ALIGNMENT);

using std::pair;

static FileWatcher *g_file_watcher = NULL;
static CriticalSection g_file_watcher_cs;

FileWatcher *FileWatcher::_instance = nullptr;

FileWatcher &FileWatcher::instance()
{
	if (!_instance)
		_instance = new FileWatcher;
	return *_instance;
}

bool FileWatcher::is_watching_file(const char *filename) const
{
	auto it = _watched_files.find(filename);
	return it != _watched_files.end() && it->second > 0;
}

void FileWatcher::add_event(DirWatch *watch, FileEvent event, uint32_t time, const string &filename, const string &new_name)
{
	DeferredFileEvent *def = ALIGNED_MALLOC(DeferredFileEvent);
	ZeroMemory(def, sizeof(DeferredFileEvent));
	*def = DeferredFileEvent(watch, event, time, filename, new_name);
	InterlockedPushEntrySList(g_file_watcher->_list_head, &def->entry);
}

void FileWatcher::DirWatch::on_completion()
{
	byte *base = _buf;
	{
		SCOPED_CS(g_file_watcher_cs);
		if (!g_file_watcher)
			return;
	}

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
					if (g_file_watcher->is_watching_file(fullname.c_str())) {
						add_event(this, kFileEventCreate, now, filename);
						_deferred_file_events_ref_count[filename]++;
					}
				}
				break;
			case FILE_ACTION_MODIFIED:
				{
					if (g_file_watcher->is_watching_file(fullname.c_str())) {
						add_event(this, kFileEventModify, now, filename);
						_deferred_file_events_ref_count[filename]++;
					}
				}
				break;
			case FILE_ACTION_REMOVED:
				{
					if (g_file_watcher->is_watching_file(fullname.c_str())) {
						add_event(this, kFileEventDelete, now, filename);
						_deferred_file_events_ref_count[filename]++;
					}
				}
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
				{
					if (g_file_watcher->is_watching_file(fullname.c_str())) {
						old_name = filename;
					}
				}
				break;
			case FILE_ACTION_RENAMED_NEW_NAME:
				if (!old_name.empty()) {
					add_event(this, kFileEventRename, now, old_name, filename);
					_deferred_file_events_ref_count[old_name]++;
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

bool FileWatcher::DirWatch::start_watch()
{
	return !!ReadDirectoryChangesW(_handle, _buf, sizeof(_buf), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, 
		NULL, &_overlapped, FileWatcher::on_completion);
}

FileWatcher::FileWatcher() 
	: _thread(INVALID_HANDLE_VALUE) 
	, _terminating(false)
{
	SCOPED_CS(g_file_watcher_cs);
	g_file_watcher = this;

	_list_head = ALIGNED_MALLOC(SLIST_HEADER);
	InitializeSListHead(_list_head);
}

UINT FileWatcher::threadProc(void *userdata)
{
	const DWORD cFileInactivity = 500;

	FileWatcher *self = (FileWatcher *)userdata;
	while (true) {
		DWORD res = SleepEx(250, TRUE);
		if (self->_terminating)
			break;

		const DWORD now = timeGetTime();

		// process any deferred file events
		while (QueryDepthSList(self->_list_head)) {
			SLIST_ENTRY *p = InterlockedPopEntrySList(self->_list_head);
			DeferredFileEvent *cur = (DeferredFileEvent *)p;

			if (now - cur->time < cFileInactivity) {
				InterlockedPushEntrySList(self->_list_head, p);
				break;
			}

			DirWatch *watch = cur->watch;
			if (--watch->_deferred_file_events_ref_count[cur->filename] == 0) {
				auto it_file = watch->_file_watches.find(cur->filename);
				if (it_file != watch->_file_watches.end()) {
					// invoke all the callbacks on the current file
					for (auto it_callback = it_file->second.begin(); it_callback != it_file->second.end(); ++it_callback)
						(*it_callback)(cur->event, cur->filename.c_str(), cur->new_name.c_str());
				}
			}

			_aligned_free(p);
		}
	}
	return 0;
}

void FileWatcher::on_completion(DWORD error_code, DWORD bytes_transfered, OVERLAPPED *overlapped)
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

void FileWatcher::terminate_apc(ULONG_PTR data)
{
	FileWatcher *self = (FileWatcher *)(data);
	self->_terminating = true;
	{
		SCOPED_CS(g_file_watcher_cs);
		g_file_watcher = NULL;
	}
	for (auto i = self->_dir_watches.begin(); i != self->_dir_watches.end(); ++i) {
		DirWatch *watch = i->second;
		CancelIo(watch->_handle);
	}
	self->_dir_watches.clear();
}

void FileWatcher::add_watch_apc(ULONG_PTR data)
{
	FileWatcher *self = (FileWatcher *)(data);
	SCOPED_CS(g_file_watcher_cs);

	// process the deferred add list
	DeferredAdds &adds = self->_deferred_adds;
	while (!adds.empty()) {

		pair<string, FileChanged> cur = adds.front();
		adds.pop_front();

		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath(cur.first.c_str(), drive, dir, fname, ext);
		string path(string(drive) + dir);
		string filename(string(fname) + ext);

		// Check if we need to create a new watch
		DirWatches &dir_watches = self->_dir_watches;
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

		dir_watches[path]->_file_watches[filename].push_back(cur.second);
	}
}

void FileWatcher::remove_watch_apc(ULONG_PTR data)
{
	pair<FileWatcher *, FileChanged > *p = (pair<FileWatcher *, FileChanged > *)data;
	FileWatcher *self = p->first;

	for (auto it_dir = self->_dir_watches.begin(); it_dir != self->_dir_watches.end(); ++it_dir) {
		DirWatch *d = it_dir->second;
		for (auto it_file = d->_file_watches.begin(); it_file != d->_file_watches.end(); ++it_file) {
			for (auto it_callback = it_file->second.begin(); it_callback != it_file->second.end(); ) {
				if (*it_callback == p->second) {
					it_callback = it_file->second.erase(it_callback);
				} else {
					++it_callback;
				}
			}
		}
	}

	delete p;
}

bool FileWatcher::lazy_create_worker_thread()
{
	if (_thread != INVALID_HANDLE_VALUE)
		return true;
	_thread = (HANDLE)_beginthreadex(NULL, 0, &FileWatcher::threadProc, this, 0, NULL);
	return _thread != INVALID_HANDLE_VALUE;
}



void FileWatcher::add_file_watch(const char *filename, const FileChanged &fn)
{
	if (!lazy_create_worker_thread())
		return;


	SCOPED_CS(g_file_watcher_cs);
	_deferred_adds.push_back(std::make_pair(filename, fn));
	_watched_files[filename]++;
	_files_by_callback[fn].push_back(filename);
	QueueUserAPC(&FileWatcher::add_watch_apc, _thread, (ULONG_PTR)this);
}

void FileWatcher::remove_watch(const FileChanged &fn)
{
	SCOPED_CS(g_file_watcher_cs);

	// dec the ref count on all the files watches by this callback
	auto it = _files_by_callback.find(fn);
	if (it == _files_by_callback.end())
		return;

	for (auto j = it->second.begin(); j != it->second.end(); ++j)
		_watched_files[*j]--;

	auto p = new pair<FileWatcher *, FileChanged>;
	p->first = this;
	p->second = fn;
	QueueUserAPC(&FileWatcher::remove_watch_apc, _thread, (ULONG_PTR)p);
}

void FileWatcher::close()
{
	if (_thread == INVALID_HANDLE_VALUE)
		return;

	QueueUserAPC(&FileWatcher::terminate_apc, _thread, (ULONG_PTR)this);
	WaitForSingleObject(_thread, INFINITE);
	_thread = INVALID_HANDLE_VALUE;
}
