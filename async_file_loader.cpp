#include "stdafx.h"
#include "async_file_loader.hpp"

using std::pair;
using std::string;
using std::tuple;
using std::make_tuple;
using std::get;
using boost::scoped_ptr;

struct LoadData {
	HANDLE h;
	CbFileLoaded cb;
	OVERLAPPED overlapped;
};

typedef pair<string, CbFileLoaded> DeferredLoad;
typedef tuple<OVERLAPPED, HANDLE, CbFileLoaded, void *, size_t, string> InProgressLoad;

struct LoaderThread {

	static void CALLBACK load_file_apc(ULONG_PTR data);
	static void CALLBACK cancel_load_apc(ULONG_PTR data);
	static void CALLBACK terminate_apc(ULONG_PTR data);
	static void CALLBACK on_completion(DWORD error_code, DWORD bytes_transfered, OVERLAPPED *overlapped);

	static bool _terminating;
	static UINT __stdcall thread_proc(void *userdata);
};

bool LoaderThread::_terminating = false;

UINT LoaderThread::thread_proc(void *userdata)
{
	while (true) {
		DWORD res = SleepEx(INFINITE, TRUE);
		if (_terminating)
			break;
	}

	return 0;
}

void LoaderThread::load_file_apc(ULONG_PTR data)
{
	scoped_ptr<DeferredLoad> load_data((DeferredLoad *)data);
	const string &filename = load_data->first;
	const CbFileLoaded &cb = load_data->second;
	HANDLE h = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return;

	size_t size = GetFileSize(h, NULL);
	void *buf = malloc(size);

	InProgressLoad *in_progress = new InProgressLoad(make_tuple(OVERLAPPED(), h, cb, buf, size, filename));
	OVERLAPPED *o = &get<0>(*in_progress);
	ZeroMemory(o, sizeof(OVERLAPPED));
	o->hEvent = in_progress;
	ReadFileEx(h, buf, size, o, &LoaderThread::on_completion);
}

void LoaderThread::cancel_load_apc(ULONG_PTR data)
{

}

void LoaderThread::terminate_apc(ULONG_PTR data)
{

}

void LoaderThread::on_completion(DWORD error_code, DWORD bytes_transfered, OVERLAPPED *overlapped)
{
	scoped_ptr<InProgressLoad> data((InProgressLoad *)overlapped->hEvent);
	const CbFileLoaded &cb = get<2>(*data);
	const string &filename = get<5>(*data);

	if (error_code != ERROR_SUCCESS)
		cb(filename.c_str(), NULL, 0);
	else
		cb(filename.c_str(), get<3>(*data), get<4>(*data));
}

AsyncFileLoader *AsyncFileLoader::_instance = NULL;

AsyncFileLoader::AsyncFileLoader()
	: _thread(INVALID_HANDLE_VALUE)
{
}

AsyncFileLoader &AsyncFileLoader::instance()
{
	if (!_instance)
		_instance = new AsyncFileLoader;
	return *_instance;
}

bool AsyncFileLoader::lazy_create_thread()
{
	if (_thread != INVALID_HANDLE_VALUE)
		return true;

	_thread = (HANDLE)_beginthreadex(NULL, 0, &LoaderThread::thread_proc, this, 0, NULL);
	return _thread != INVALID_HANDLE_VALUE;
}

bool AsyncFileLoader::load_file(const char *filename, const CbFileLoaded &cb)
{
	if (!lazy_create_thread())
		return false;

	return !!QueueUserAPC(&LoaderThread::load_file_apc, _thread, (ULONG_PTR)new DeferredLoad(filename, cb));
}

void AsyncFileLoader::remove_callback(const char *filename, const CbFileLoaded &cb)
{

}
