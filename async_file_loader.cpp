#include "stdafx.h"
#include "async_file_loader.hpp"

struct LoaderThread {

	static void CALLBACK load_file_apc(ULONG_PTR data);
	static void CALLBACK terminate_apc(ULONG_PTR data);
	static void CALLBACK on_completion(DWORD error_Code, DWORD bytes_transfered, OVERLAPPED *overlapped);

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
	return false;
	//HANDLE h = CreateFile()
}
