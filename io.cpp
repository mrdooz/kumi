#include "stdafx.h"
#include "io.hpp"
#include "file_utils.hpp"
#include "utils.hpp"

using std::pair;

bool DiskIo::load_file(const char *filename, void **buf, size_t *len) {
	return ::load_file(filename, buf, len);
}

bool DiskIo::file_exists(const char *filename) {
	return ::file_exists(filename);
}

__time64_t DiskIo::mdate(const char *filename) {
	struct _stat s;
	_stat(filename, &s);
	return s.st_mtime;
}

bool DiskIo::load_partial(const char *filename, size_t ofs, size_t len, void **buf) {

	ScopedHandle h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
	if (h.handle() == INVALID_HANDLE_VALUE)
		return false;

	DWORD size = GetFileSize(h, NULL);
	if (ofs + len > size)
		return false;

	// if no buffer is supplied, allocate a new one
	const bool mem_supplied = *buf != NULL;
	if (!mem_supplied)
		*buf = new uint8_t[len];

	DWORD res;
	if (SetFilePointer(h, ofs, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		if (!mem_supplied)
			delete [] exch_null(*buf);
		return false;
	}

	if (!ReadFile(h, *buf, len, &res, NULL)) {
		DWORD err = GetLastError();
		if (!mem_supplied)
			delete [] exch_null(*buf);
		return false;
	}

	return true;
}


struct FindFileData {
	HANDLE handle;
	WIN32_FIND_DATAA data;
	string path;
};

bool DiskIo::find_first(const char *filemask, string *filename, void **token) {
	FindFileData *data = new FindFileData;
	split_path(filemask, NULL, &data->path, NULL, NULL);

	data->handle = FindFirstFileA(filemask, &data->data);
	if (data->handle == INVALID_HANDLE_VALUE) {
		delete data;
		return false;
	}
	*filename = data->path + data->data.cFileName;
	*token = (void *)data;
	return true;
}

bool DiskIo::find_next(void *token, string *filename) {
	FindFileData *data = (FindFileData *)token;
	if (!FindNextFileA(data->handle, &data->data))
		return false;
	*filename = data->path + data->data.cFileName;
	return true;
}

void DiskIo::find_close(void *token) {
	FindFileData *data = (FindFileData *)token;
	FindClose(data->handle);
	delete data;
}
