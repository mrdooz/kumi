#include "stdafx.h"
#include "async_file_loader.hpp"

AsyncFileLoader *AsyncFileLoader::_instance = NULL;

AsyncFileLoader::AsyncFileLoader()
	: _terminating(false)
	, _thread(INVALID_HANDLE_VALUE)
{
}

UINT AsyncFileLoader::threadProc(void *userdata)
{
	AsyncFileLoader *self = (AsyncFileLoader *)userdata;

	while (true) {
		DWORD res = SleepEx(INFINITE, TRUE);
		if (self->_terminating)
			break;
	}

	return 0;
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

	return true;
}

bool AsyncFileLoader::load_file(const char *filename, const CbFileLoaded &cb)
{
	return false;
	//HANDLE h = CreateFile()
}
